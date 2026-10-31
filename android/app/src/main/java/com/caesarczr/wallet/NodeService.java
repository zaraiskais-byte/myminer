package com.caesarczr.wallet;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;
import android.os.BatteryManager;
import android.os.PowerManager;
import android.util.Log;
import java.io.BufferedReader;
import java.io.File;
import java.io.InputStreamReader;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.util.ArrayList;
import java.util.regex.Pattern;

public class NodeService extends Service {
    static final int RPC_PORT = 38543;
    static final int P2P_PORT = 39555;
    static final String PEER = "127.0.0.1:18555";
    private static final String TAG = "CaesarNode";
    private static final String CHANNEL = "caesar_node";
    private static final Object LOCK = new Object();
    private static Process proc;
    private static volatile String lastLine = "";
    private static Process workerProc;
    private static String workerAddr = "";
    private static int workerFails = 0;
    private static long workerNextTry = 0;
    private static long workerStarted = 0;
    private static int workerThreads = 0;
    private static long lastTune = 0;
    private static boolean heatPaused = false;
    private static PowerManager.WakeLock wakeLock;
    private static final Pattern ADDR = Pattern.compile("^CZ1[0-9A-Za-z]{8,120}$");

    static boolean portOpen() {
        try (Socket s = new Socket()) {
            s.connect(new InetSocketAddress("127.0.0.1", RPC_PORT), 300);
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    static String lastLine() { return lastLine; }

    private static boolean alive(Process p) {
        if (p == null) return false;
        try { p.exitValue(); return false; } catch (IllegalThreadStateException e) { return true; }
    }

    static void ensureNode(Context ctx) {
        synchronized (LOCK) {
            if (alive(proc) || portOpen()) return;
            try {
                File bin = new File(ctx.getApplicationInfo().nativeLibraryDir, "libcaesar_web.so");
                File data = new File(ctx.getFilesDir(), "data");
                data.mkdirs();
                ArrayList<String> cmd = new ArrayList<String>();
                cmd.add(bin.getAbsolutePath());
                cmd.add("--data"); cmd.add(data.getAbsolutePath());
                cmd.add("--rpc-port"); cmd.add(String.valueOf(RPC_PORT));
                cmd.add("--port"); cmd.add(String.valueOf(P2P_PORT));
                if (PEER.length() > 0) { cmd.add("--peer"); cmd.add(PEER); }
                ProcessBuilder pb = new ProcessBuilder(cmd);
                pb.redirectErrorStream(true);
                pb.directory(ctx.getFilesDir());
                final Process p = pb.start();
                proc = p;
                new Thread(new Runnable() {
                    public void run() {
                        try {
                            BufferedReader r = new BufferedReader(new InputStreamReader(p.getInputStream()));
                            String line;
                            while ((line = r.readLine()) != null) { lastLine = line; Log.i(TAG, line); }
                        } catch (Exception ignored) {}
                    }
                }).start();
            } catch (Exception e) {
                lastLine = "spawn failed: " + e;
                Log.e(TAG, "spawn failed", e);
            }
        }
    }

    /** Picks miner threads from charging state, battery level and temperature. 0 means pause. */
    static int pickThreads(Context ctx) {
        int cores = Math.max(1, Runtime.getRuntime().availableProcessors());
        try {
            Intent b = ctx.registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
            if (b == null) return Math.max(1, cores / 2);
            int level = b.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
            int scale = b.getIntExtra(BatteryManager.EXTRA_SCALE, 100);
            int status = b.getIntExtra(BatteryManager.EXTRA_STATUS, -1);
            int temp = b.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, 0);
            boolean charging = status == BatteryManager.BATTERY_STATUS_CHARGING
                    || status == BatteryManager.BATTERY_STATUS_FULL;
            int pct = (level >= 0 && scale > 0) ? (level * 100 / scale) : 50;
            if (temp >= 450) heatPaused = true; else if (temp < 420) heatPaused = false;
            if (heatPaused) return 0;
            int t;
            if (charging) t = Math.max(1, cores - 1);
            else if (pct >= 50) t = Math.max(1, cores / 2);
            else if (pct >= 25) t = 1;
            else return 0;
            if (temp >= 420) t = Math.max(1, t / 2);
            return t;
        } catch (Exception e) {
            return Math.max(1, cores / 2);
        }
    }

    private static void releaseWake() {
        try { if (wakeLock != null && wakeLock.isHeld()) wakeLock.release(); } catch (Exception ignored) {}
    }

    /** Starts (or restarts) the real miner for the given wallet address. */
    static void ensureWorker(Context ctx, String address) {
        if (address == null || !ADDR.matcher(address).matches()) return;
        synchronized (LOCK) {
            long now = System.currentTimeMillis();
            boolean running = workerProc != null && alive(workerProc);
            int target = pickThreads(ctx);
            if (target == 0) {
                if (running) {
                    workerProc.destroy();
                    workerProc = null;
                    releaseWake();
                    lastLine = "miner paused: battery or heat";
                }
                return;
            }
            if (running && target != workerThreads && now - lastTune > 120000L) {
                workerProc.destroy();
                workerProc = null;
                running = false;
                workerNextTry = 0;
                lastTune = now;
                lastLine = "miner retuned to " + target + " threads";
            }
            if (workerProc != null && !running) {
                releaseWake();
                if (now - workerStarted < 30000L) workerFails++; else workerFails = 0;
                workerProc = null;
                workerNextTry = now + Math.min(300000L, 5000L << Math.min(workerFails, 6));
                lastLine = "worker exited, retry with backoff";
            }
            if (running && address.equals(workerAddr)) {
                if (wakeLock != null) wakeLock.acquire(600000L);
                return;
            }
            if (now < workerNextTry) return;
            if (workerProc != null) { workerProc.destroy(); workerProc = null; }
            try {
                File bin = new File(ctx.getApplicationInfo().nativeLibraryDir, "libcaesar_worker.so");
                if (!bin.exists()) { lastLine = "worker binary missing"; return; }
                int threads = target;
                ArrayList<String> cmd = new ArrayList<String>();
                cmd.add(bin.getAbsolutePath());
                cmd.add("--pool"); cmd.add("http://127.0.0.1:" + RPC_PORT);
                cmd.add("--address"); cmd.add(address);
                cmd.add("--name"); cmd.add("phone");
                cmd.add("--threads"); cmd.add(String.valueOf(threads));
                ProcessBuilder pb = new ProcessBuilder(cmd);
                pb.redirectErrorStream(true);
                pb.directory(ctx.getFilesDir());
                final Process p = pb.start();
                workerProc = p;
                workerAddr = address;
                workerStarted = now;
                workerThreads = target;
                if (wakeLock == null) {
                    PowerManager pm = (PowerManager) ctx.getSystemService(Context.POWER_SERVICE);
                    wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "caesar:miner");
                    wakeLock.setReferenceCounted(false);
                }
                wakeLock.acquire(600000L);
                new Thread(new Runnable() {
                    public void run() {
                        try {
                            BufferedReader r = new BufferedReader(new InputStreamReader(p.getInputStream()));
                            String line;
                            while ((line = r.readLine()) != null) { lastLine = line; Log.i("CaesarWorker", line); }
                        } catch (Exception ignored) {}
                    }
                }).start();
            } catch (Exception e) {
                workerFails++;
                workerNextTry = now + Math.min(300000L, 5000L << Math.min(workerFails, 6));
                lastLine = "worker spawn failed: " + e;
                Log.e(TAG, "worker spawn failed", e);
            }
        }
    }

    @Override
    public IBinder onBind(Intent intent) { return null; }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        try {
            NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
            Notification.Builder b;
            if (Build.VERSION.SDK_INT >= 26) {
                nm.createNotificationChannel(new NotificationChannel(CHANNEL, "Caesar node", NotificationManager.IMPORTANCE_LOW));
                b = new Notification.Builder(this, CHANNEL);
            } else {
                b = new Notification.Builder(this);
            }
            b.setContentTitle("Caesar CZR").setContentText("Node is running")
             .setSmallIcon(android.R.drawable.stat_notify_sync).setOngoing(true);
            Notification n = b.build();
            if (Build.VERSION.SDK_INT >= 29) startForeground(1, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
            else startForeground(1, n);
        } catch (Exception e) {
            Log.e(TAG, "startForeground failed", e);
        }
        new Thread(new Runnable() { public void run() { ensureNode(NodeService.this); } }).start();
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        synchronized (LOCK) {
            if (proc != null) { proc.destroy(); proc = null; }
            if (workerProc != null) { workerProc.destroy(); workerProc = null; }
            releaseWake();
        }
        super.onDestroy();
    }
}
