package com.caesarczr.wallet;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import java.io.File;
import java.net.Socket;

public class MainActivity extends Activity {
    private static final String TAG = "CaesarCZR";
    private static final int RPC_PORT = 8332;
    private static final int P2P_PORT = 18555;

    private Process nodeProcess;
    private WebView webView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        String nativeDir = getApplicationInfo().nativeLibraryDir;
        File binary = new File(nativeDir, "libcaesar_web.so");
        Log.i(TAG, "binary path: " + binary.getAbsolutePath());
        Log.i(TAG, "binary exists: " + binary.exists() + " canExecute: " + binary.canExecute());

        if (!binary.exists()) {
            Log.e(TAG, "binary missing in nativeLibraryDir");
        }

        File dataDir = new File(getFilesDir(), "data");
        dataDir.mkdirs();

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
            Log.i(TAG, "node process started pid=" + nodeProcess.pid());
        } catch (Exception e) {
            Log.e(TAG, "spawn failed", e);
        }

        waitForServer();

        webView = new WebView(this);
        WebSettings s = webView.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setLoadWithOverviewMode(true);
        s.setUseWideViewPort(true);
        webView.setWebViewClient(new WebViewClient());
        webView.loadUrl("http://127.0.0.1:" + RPC_PORT);
        setContentView(webView);
    }

    private void waitForServer() {
        for (int i = 0; i < 100; i++) {
            try (Socket sock = new Socket("127.0.0.1", RPC_PORT)) {
                Log.i(TAG, "server ready after " + i + " tries");
                return;
            } catch (Exception ignored) {
                try { Thread.sleep(100); } catch (InterruptedException e) { return; }
            }
        }
        Log.w(TAG, "server did not become ready");
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
