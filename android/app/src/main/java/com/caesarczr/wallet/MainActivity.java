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
    private static final String URL = "http://127.0.0.1:" + NodeService.RPC_PORT;
    private FrameLayout root;
    private TextView status;
    private TextView dbg;
    private WebView webView;
    private Handler ui;
    private boolean waiting = false;
    private boolean pageLoaded = false;

    private final Runnable minerTick = new Runnable() {
        @Override
        public void run() {
            try {
                if (pageLoaded && webView != null && NodeService.portOpen()) {
                    webView.evaluateJavascript(
                        "(function(){try{var l=document.getElementById('lock');var a=document.getElementById('app');if(!a||getComputedStyle(a).display==='none')return '';if(l&&getComputedStyle(l).display!=='none')return '';return localStorage.getItem('czr_address')||''}catch(e){return ''}})()",
                        new ValueCallback<String>() {
                            @Override
                            public void onReceiveValue(String v) {
                                if (v == null) return;
                                final String a = v.replace("\"", "").trim();
                                if (a.length() == 0) return;
                                new Thread(new Runnable() {
                                    public void run() { NodeService.ensureWorker(MainActivity.this, a); }
                                }).start();
                            }
                        });
                }
            } catch (Exception ignored) {}
            if (dbg != null) dbg.setText(NodeService.debugState(MainActivity.this));
            ui.postDelayed(this, 5000);
        }
    };

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
        status.setBackgroundColor(0xFF0A1020);
        status.setTextColor(0xFFE8C45A);
        status.setCompoundDrawablesWithIntrinsicBounds(0, R.drawable.splash_logo, 0, 0);
        status.setCompoundDrawablePadding(24);
        status.setText("Starting Caesar node...");
        attachNewWebView();
        root.addView(status, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        setContentView(root);
        dbg = new TextView(this);
        dbg.setTextSize(10);
        dbg.setTextColor(0xFFFFFF00);
        dbg.setBackgroundColor(0xCC000000);
        dbg.setPadding(8, 4, 8, 4);
        root.addView(dbg, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.BOTTOM));
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
        ui.removeCallbacks(minerTick);
        ui.postDelayed(minerTick, 5000);
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
