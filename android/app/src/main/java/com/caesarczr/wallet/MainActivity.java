package com.caesarczr.wallet;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.CookieManager;
import android.webkit.RenderProcessGoneDetail;
import android.webkit.ValueCallback;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.FrameLayout;
import android.widget.TextView;

public class MainActivity extends Activity {
    private static final String URL = "https://czr-node.taila0a431.ts.net";
    private FrameLayout root;
    private TextView status;
    private WebView webView;
    private Handler ui;
    private boolean waiting = false;
    private boolean pageLoaded = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        ui = new Handler(Looper.getMainLooper());
        root = new FrameLayout(this);
        root.setFitsSystemWindows(true);
        status = new TextView(this);
        status.setTextSize(14);
        status.setPadding(32, 32, 32, 32);
        status.setGravity(Gravity.CENTER);
        status.setBackgroundColor(0xFFFFFFFF);
        status.setText("Starting Caesar node...");
        attachNewWebView();
        root.addView(status, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        setContentView(root);
        if (Build.VERSION.SDK_INT >= 33
                && checkSelfPermission("android.permission.POST_NOTIFICATIONS") != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{"android.permission.POST_NOTIFICATIONS"}, 1);
        }
        startNode();
    }

    private void attachNewWebView() {
        if (webView != null) {
            root.removeView(webView);
            try { webView.destroy(); } catch (Exception ignored) {}
        }
        webView = new WebView(this);
        WebSettings s = webView.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setLoadWithOverviewMode(true);
        s.setUseWideViewPort(true);
        s.setMixedContentMode(WebSettings.MIXED_CONTENT_ALWAYS_ALLOW);
        CookieManager cm = CookieManager.getInstance();
        cm.setAcceptCookie(true);
        cm.setAcceptThirdPartyCookies(webView, true);
        webView.setWebViewClient(new WebViewClient() {
            @Override
            @TargetApi(26)
            public boolean onRenderProcessGone(WebView view, RenderProcessGoneDetail detail) {
                ui.post(new Runnable() {
                    public void run() {
                        attachNewWebView();
                        if (NodeService.portOpen()) webView.loadUrl(URL);
                    }
                });
                return true;
            }
        });
        root.addView(webView, 0, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
    }

    private void startNode() {
        Intent i = new Intent(this, NodeService.class);
        try {
            if (Build.VERSION.SDK_INT >= 26) startForegroundService(i); else startService(i);
        } catch (Exception e) {
            status.setText("service start failed: " + e);
        }
        waitAndLoad();
    }

    private void waitAndLoad() {
        if (waiting) return;
        waiting = true;
        status.setVisibility(View.VISIBLE);
        new Thread(new Runnable() {
            public void run() {
                long end = System.currentTimeMillis() + 180000L;
                boolean ok = false;
                while (System.currentTimeMillis() < end) {
                    if (NodeService.portOpen()) { ok = true; break; }
                    final String l = NodeService.lastLine();
                    ui.post(new Runnable() {
                        public void run() { status.setText("Starting Caesar node...\n" + l); }
                    });
                    try { Thread.sleep(500); } catch (InterruptedException e) { break; }
                }
                final boolean fok = ok;
                ui.post(new Runnable() {
                    public void run() {
                        waiting = false;
                        if (fok) {
                            status.setVisibility(View.GONE);
                            webView.loadUrl(URL);
                            pageLoaded = true;
                        } else {
                            status.setText("Node did not start.\n" + NodeService.lastLine()
                                    + "\nReopen the app to retry.");
                        }
                    }
                });
            }
        }).start();
    }

    private void checkBlank() {
        webView.evaluateJavascript("(document.body&&document.body.innerText||'').length",
                new ValueCallback<String>() {
                    @Override
                    public void onReceiveValue(String v) {
                        if (v == null || v.equals("0") || v.equals("null")) webView.reload();
                    }
                });
    }

    @Override
    protected void onResume() {
        super.onResume();
        webView.onResume();
        webView.resumeTimers();
        if (!NodeService.portOpen()) {
            if (!waiting) startNode();
        } else if (pageLoaded) {
            checkBlank();
        }
    }

    @Override
    protected void onPause() {
        CookieManager.getInstance().flush();
        if (webView != null) webView.onPause();
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        CookieManager.getInstance().flush();
        if (webView != null) {
            root.removeView(webView);
            webView.destroy();
            webView = null;
        }
        super.onDestroy();
    }

    @Override
    public void onBackPressed() {
        if (webView != null && webView.canGoBack()) webView.goBack();
        else super.onBackPressed();
    }
}
