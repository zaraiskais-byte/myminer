package com.caesarczr.wallet;
import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
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
        File filesDir = getFilesDir();
        File binary = new File(filesDir, "caesar_web");
        if (!binary.exists()) {
            try { extractAsset("caesar_web", binary); } catch (Exception e) { Log.e(TAG, "extract failed", e); }
        }
        binary.setExecutable(true, false);
        binary.setReadable(true, false);
        File dataDir = new File(filesDir, "data");
        dataDir.mkdirs();
        try {
            ProcessBuilder pb = new ProcessBuilder(binary.getAbsolutePath(), "--data", dataDir.getAbsolutePath(), "--rpc-port", String.valueOf(RPC_PORT), "--port", String.valueOf(P2P_PORT));
            pb.redirectErrorStream(true);
            pb.directory(filesDir);
            nodeProcess = pb.start();
        } catch (Exception e) { Log.e(TAG, "spawn failed", e); }
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
    private void extractAsset(String name, File dest) throws Exception {
        try (InputStream in = getAssets().open(name); OutputStream out = new FileOutputStream(dest)) {
            byte[] buf = new byte[8192];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
        }
    }
    private void waitForServer() {
        for (int i = 0; i < 60; i++) {
            try (Socket sock = new Socket("127.0.0.1", RPC_PORT)) { return; }
            catch (Exception ignored) { try { Thread.sleep(100); } catch (InterruptedException ignored2) {} }
        }
    }
    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (nodeProcess != null) { nodeProcess.destroy(); nodeProcess = null; }
    }
    @Override
    public void onBackPressed() {
        if (webView != null && webView.canGoBack()) webView.goBack();
        else super.onBackPressed();
    }
}
