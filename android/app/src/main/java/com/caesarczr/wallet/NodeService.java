package com.caesarczr.wallet;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;
import android.util.Log;
import java.io.BufferedReader;
import java.io.File;
import java.io.InputStreamReader;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.util.ArrayList;

public class NodeService extends Service {
    static final int RPC_PORT = 38543;
    static final int P2P_PORT = 39555;
    static final String PEER = "127.0.0.1:18555";
    private static final String TAG = "CaesarNode";
    private static final String CHANNEL = "caesar_node";
    private static final Object LOCK = new Object();
    private static Process proc;
    private static volatile String lastLine = "";

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
        }
        super.onDestroy();
    }
}
