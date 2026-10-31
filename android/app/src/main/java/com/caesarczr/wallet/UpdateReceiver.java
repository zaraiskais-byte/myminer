package com.caesarczr.wallet;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInstaller;

public class UpdateReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context ctx, Intent i) {
        int st = i.getIntExtra(PackageInstaller.EXTRA_STATUS, -1);
        if (st == PackageInstaller.STATUS_PENDING_USER_ACTION) {
            Intent a = i.getParcelableExtra(Intent.EXTRA_INTENT);
            if (a != null) {
                a.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                try { ctx.startActivity(a); } catch (Exception ignored) {}
            }
        }
    }
}
