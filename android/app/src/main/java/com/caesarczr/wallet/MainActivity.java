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

    private static final boolean AUTO_UPDATE = true;
    private boolean installing = false;

    private void installUpdate(final String url) {
        if (installing) return;
        try {
            if (Build.VERSION.SDK_INT >= 26 && !getPackageManager().canRequestPackageInstalls()) {
                startActivity(new Intent(android.provider.Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                    android.net.Uri.parse("package:" + getPackageName())));
                return;
            }
        } catch (Exception ignored) {}
        installing = true;
        new Thread(new Runnable() {
            public void run() {
                java.net.HttpURLConnection c = null;
                try {
                    c = (java.net.HttpURLConnection) new java.net.URL(url).openConnection();
                    c.setInstanceFollowRedirects(true);
                    c.setConnectTimeout(10000);
                    c.setReadTimeout(30000);
                    if (c.getResponseCode() != 200) return;
                    android.content.pm.PackageInstaller pi = getPackageManager().getPackageInstaller();
                    android.content.pm.PackageInstaller.SessionParams sp =
                        new android.content.pm.PackageInstaller.SessionParams(
                            android.content.pm.PackageInstaller.SessionParams.MODE_FULL_INSTALL);
                    if (Build.VERSION.SDK_INT >= 31) {
                        sp.setRequireUserAction(android.content.pm.PackageInstaller.SessionParams.USER_ACTION_NOT_REQUIRED);
                    }
                    int id = pi.createSession(sp);
                    android.content.pm.PackageInstaller.Session ses = pi.openSession(id);
                    try {
                        java.io.OutputStream os = ses.openWrite("caesar.apk", 0, -1);
                        java.io.InputStream in = c.getInputStream();
                        byte[] buf = new byte[65536];
                        int n;
                        while ((n = in.read(buf)) > 0) os.write(buf, 0, n);
                        ses.fsync(os);
                        os.close();
                        in.close();
                        Intent it = new Intent(MainActivity.this, UpdateReceiver.class);
                        int fl = android.app.PendingIntent.FLAG_UPDATE_CURRENT;
                        if (Build.VERSION.SDK_INT >= 31) fl |= android.app.PendingIntent.FLAG_MUTABLE;
                        android.app.PendingIntent pend = android.app.PendingIntent.getBroadcast(MainActivity.this, id, it, fl);
                        ses.commit(pend.getIntentSender());
                    } finally {
                        ses.close();
                    }
                } catch (Exception ignored) {
                } finally {
                    installing = false;
                    if (c != null) c.disconnect();
                }
            }
        }).start();
    }

    private long lastUpdateCheck = 0;

    private void checkUpdate() {
        final long now = System.currentTimeMillis();
        if (now - lastUpdateCheck < 6L * 3600L * 1000L) return;
        lastUpdateCheck = now;
        new Thread(new Runnable() {
            public void run() {
                java.net.HttpURLConnection c = null;
                try {
                    c = (java.net.HttpURLConnection) new java.net.URL(
                        "https://api.github.com/repos/zaraiskais-byte/myminer/releases/latest").openConnection();
                    c.setConnectTimeout(5000);
                    c.setReadTimeout(8000);
                    c.setRequestProperty("Accept", "application/vnd.github+json");
                    if (c.getResponseCode() != 200) return;
                    java.io.BufferedReader r = new java.io.BufferedReader(
                        new java.io.InputStreamReader(c.getInputStream()));
                    StringBuilder sb = new StringBuilder();
                    String line;
                    while ((line = r.readLine()) != null && sb.length() < 200000) sb.append(line);
                    r.close();
                    java.util.regex.Matcher t = java.util.regex.Pattern
                        .compile("\"tag_name\"\\s*:\\s*\"[^\"]*?(\\d+)\"").matcher(sb);
                    java.util.regex.Matcher u = java.util.regex.Pattern
                        .compile("\"browser_download_url\"\\s*:\\s*\"([^\"]+\\.apk)\"").matcher(sb);
                    if (!t.find() || !u.find()) return;
                    final int latest = Integer.parseInt(t.group(1));
                    final String url = u.group(1);
                    String vn = getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
                    int cur = Integer.parseInt(vn.substring(vn.lastIndexOf('.') + 1));
                    if (latest <= cur) return;
                    if (AUTO_UPDATE) { installUpdate(url); return; }
                    ui.post(new Runnable() {
                        public void run() {
                            try {
                                new android.app.AlertDialog.Builder(MainActivity.this)
                                    .setTitle("Caesar CZR")
                                    .setMessage("New version 3.0." + latest + " is available.")
                                    .setPositiveButton("Update", new android.content.DialogInterface.OnClickListener() {
                                        public void onClick(android.content.DialogInterface d, int w) {
                                            startActivity(new Intent(Intent.ACTION_VIEW, android.net.Uri.parse(url)));
                                        }
                                    })
                                    .setNegativeButton("Later", null)
                                    .show();
                            } catch (Exception ignored) {}
                        }
                    });
                } catch (Exception ignored) {
                } finally {
                    if (c != null) c.disconnect();
                }
            }
        }).start();
    }

    private android.speech.tts.TextToSpeech czrTts;
    private boolean czrTtsReady = false;

    public class CzrTtsBridge {
        @android.webkit.JavascriptInterface
        public void speak(final String text, final String lang) {
            ui.post(new Runnable() {
                public void run() {
                    try {
                        if (czrTts == null) {
                            czrTts = new android.speech.tts.TextToSpeech(MainActivity.this,
                                new android.speech.tts.TextToSpeech.OnInitListener() {
                                    public void onInit(int st) {
                                        czrTtsReady = (st == android.speech.tts.TextToSpeech.SUCCESS);
                                        czrSpeakNow(text, lang);
                                    }
                                });
                        } else {
                            czrSpeakNow(text, lang);
                        }
                    } catch (Exception ignored) {}
                }
            });
        }
    }

    private void czrSpeakNow(String text, String lang) {
        if (czrTts == null || !czrTtsReady) return;
        java.util.Locale loc = "ar".equals(lang) ? new java.util.Locale("ar") : java.util.Locale.US;
        int r = czrTts.setLanguage(loc);
        if (r == android.speech.tts.TextToSpeech.LANG_MISSING_DATA
                || r == android.speech.tts.TextToSpeech.LANG_NOT_SUPPORTED) return;
        czrTts.speak(text, android.speech.tts.TextToSpeech.QUEUE_FLUSH, null, "czr");
    }

    
private void attachNewWebView() {
        if (webView != null) {
            root.removeView(webView);
            try { webView.destroy(); } catch (Exception ignored) {}
        }
        webView = new WebView(this);
        webView.addJavascriptInterface(new CzrTtsBridge(), "CzrTTS");
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
        checkUpdate();
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
