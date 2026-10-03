package com.caesarczr.wallet;

import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.widget.ScrollView;
import android.widget.TextView;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileWriter;
import java.io.InputStreamReader;
import java.net.Socket;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

public class MainActivity extends Activity {
    private static final String TAG = "CaesarCZR";
    private static final int RPC_PORT = 8332;
    private static final int P2P_PORT = 18555;

    private Process nodeProcess;
    private WebView webView;
    private TextView logView;
    private StringBuilder logBuffer = new StringBuilder();
    private Handler mainHandler;
    private File logFile;

    private void appendLog(String msg) {
        Log.i(TAG, msg);
        String ts = new SimpleDateFormat("HH:mm:ss", Locale.US).format(new Date());
        String line = ts + "  " + msg + "\n";
        logBuffer.append(line);
        if (logView != null) {
            logView.setText(logBuffer.toString());
        }
        try {
            if (logFile == null) {
                File dir = getExternalFilesDir(null);
                if (dir != null) {
                    if (!dir.exists()) dir.mkdirs();
                    logFile = new File(dir, "app.log");
                }
            }
            if (logFile != null) {
                FileWriter fw = new FileWriter(logFile, true);
                fw.write(line);
                fw.close();
            }
        } catch (Exception ignored) {}
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mainHandler = new Handler(Looper.getMainLooper());

        ScrollView sv = new ScrollView(this);
        logView = new TextView(this);
        logView.setTextSize(11);
        logView.setPadding(24, 24, 24, 24);
        logView.setTextIsSelectable(true);
        sv.addView(logView);
        setContentView(sv);

        appendLog("=== Caesar CZR diagnostics ===");
        appendLog("sdk: " + Build.VERSION.SDK_INT);
        appendLog("abi: " + Build.SUPPORTED_ABIS[0]);

        String nativeDir = getApplicationInfo().nativeLibraryDir;
        appendLog("nativeLibraryDir: " + nativeDir);
        File binary = new File(nativeDir, "libcaesar_web.so");
        appendLog("exists: " + binary.exists());
        appendLog("length: " + binary.length());
        appendLog("canExecute: " + binary.canExecute());
        appendLog("canRead: " + binary.canRead());

        if (!binary.exists()) {
            appendLog("FATAL: binary missing");
            return;
        }

        File dataDir = new File(getFilesDir(), "data");
        dataDir.mkdirs();
        appendLog("dataDir: " + dataDir.getAbsolutePath());

        new Thread(new Runnable() {
            public void run() {
                startNodeAndLoad();
            }
        }).start();
    }

    private void startNodeAndLoad() {
        String nativeDir = getApplicationInfo().nativeLibraryDir;
        File binary = new File(nativeDir, "libcaesar_web.so");
        File dataDir = new File(getFilesDir(), "data");

        try {
            ProcessBuilder pb = new ProcessBuilder(
                binary.getAbsolutePath(),
                "--data", dataDir.getAbsolutePath(),
                "--rpc-port", String.valueOf(RPC_PORT),
                "--port", String.valueOf(P2P_PORT)
            );
            pb.redirectErrorStream(true);
            pb.directory(getFilesDir());
            nodeProcess = pb.start();
            mainHandler.post(new Runnable() { public void run() { appendLog("node started"); }});

            final BufferedReader reader = new BufferedReader(new InputStreamReader(nodeProcess.getInputStream()));
            new Thread(new Runnable() {
                public void run() {
                    try {
                        String line;
                        while ((line = reader.readLine()) != null) {
                            final String l = line;
                            mainHandler.post(new Runnable() { public void run() { appendLog("NODE: " + l); }});
                        }
                        mainHandler.post(new Runnable() { public void run() { appendLog("node stdout closed"); }});
                    } catch (Exception e) {
                        final String em = e.toString();
                        mainHandler.post(new Runnable() { public void run() { appendLog("reader err: " + em); }});
                    }
                }
            }).start();
        } catch (Exception e) {
            final String em = e.toString();
            mainHandler.post(new Runnable() { public void run() { appendLog("spawn failed: " + em); }});
        }

        boolean ready = false;
        for (int i = 0; i < 150; i++) {
            try (Socket sock = new Socket("127.0.0.1", RPC_PORT)) {
                final int tries = i;
                mainHandler.post(new Runnable() { public void run() { appendLog("server ready after " + tries + " tries"); }});
                ready = true;
                break;
            } catch (Exception ignored) {
                try { Thread.sleep(100); } catch (InterruptedException e) { return; }
            }
        }

        if (!ready) {
            mainHandler.post(new Runnable() { public void run() { appendLog("server did not become ready"); }});
        }

        final boolean finalReady = ready;
        mainHandler.post(new Runnable() {
            public void run() {
                loadWebView(finalReady);
            }
        });
    }

    private void loadWebView(boolean serverReady) {
        appendLog("switching to WebView, ready=" + serverReady);
        try {
            webView = new WebView(MainActivity.this);
            WebSettings s = webView.getSettings();
            s.setJavaScriptEnabled(true);
            s.setDomStorageEnabled(true);
            s.setLoadWithOverviewMode(true);
            s.setUseWideViewPort(true);
            webView.setWebViewClient(new WebViewClient());
            webView.loadUrl("http://127.0.0.1:" + RPC_PORT);
            setContentView(webView);
        } catch (Exception e) {
            appendLog("webview err: " + e.toString());
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (nodeProcess != null) {
            nodeProcess.destroy();
            nodeProcess = null;
        }
    }

    @Override
    public void onBackPressed() {
        if (webView != null && webView.canGoBack()) webView.goBack();
        else super.onBackPressed();
    }
}
