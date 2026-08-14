package com.gogs.ultimatedumper;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.view.Gravity;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

public class MainActivity extends Activity {
    private static final int OVERLAY_PERMISSION_REQ_CODE = 1234;

    private EditText urlEditText;
    private TextView statusTextView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        SharedPreferences prefs = getSharedPreferences(FloatingModMenuService.PREF_NAME, MODE_PRIVATE);
        String savedUrl = prefs.getString(FloatingModMenuService.PREF_KEY_URL, FloatingModMenuService.DEFAULT_CONFIG_URL);

        // Build Launcher Layout Programmatically
        ScrollView scrollView = new ScrollView(this);
        LinearLayout mainLayout = new LinearLayout(this);
        mainLayout.setOrientation(LinearLayout.VERTICAL);
        mainLayout.setPadding(40, 60, 40, 60);

        GradientDrawable bg = new GradientDrawable();
        bg.setColor(Color.parseColor("#0F172A"));
        mainLayout.setBackground(bg);

        // Title Header
        TextView titleTv = new TextView(this);
        titleTv.setText("ARK ULTIMATE MOD MENU");
        titleTv.setTextColor(Color.parseColor("#60A5FA"));
        titleTv.setTextSize(22);
        titleTv.setGravity(Gravity.CENTER);
        titleTv.setPadding(0, 20, 0, 10);
        mainLayout.addView(titleTv);

        TextView subTitleTv = new TextView(this);
        subTitleTv.setText("Cloud-Controlled Online Overlay APK v3.0\nAuthor: GoGs Ultimate GSV");
        subTitleTv.setTextColor(Color.parseColor("#94A3B8"));
        subTitleTv.setTextSize(13);
        subTitleTv.setGravity(Gravity.CENTER);
        subTitleTv.setPadding(0, 0, 0, 40);
        mainLayout.addView(subTitleTv);

        // Server URL Setting Label
        TextView urlLabel = new TextView(this);
        urlLabel.setText("🌐 URL Server Cấu Hình Online (Config Server):");
        urlLabel.setTextColor(Color.WHITE);
        urlLabel.setTextSize(13);
        urlLabel.setPadding(0, 10, 0, 10);
        mainLayout.addView(urlLabel);

        urlEditText = new EditText(this);
        urlEditText.setText(savedUrl);
        urlEditText.setTextColor(Color.WHITE);
        urlEditText.setHintTextColor(Color.GRAY);
        urlEditText.setTextSize(12);
        urlEditText.setBackgroundColor(Color.parseColor("#1E293B"));
        urlEditText.setPadding(20, 20, 20, 20);
        mainLayout.addView(urlEditText);

        // Status View
        statusTextView = new TextView(this);
        statusTextView.setText("Trạng thái: Sẵn sàng khởi chạy");
        statusTextView.setTextColor(Color.parseColor("#34D399"));
        statusTextView.setGravity(Gravity.CENTER);
        statusTextView.setPadding(0, 30, 0, 30);
        mainLayout.addView(statusTextView);

        // Button: Start Service
        Button btnStart = createButton("🚀 KÍCH HOẠT MENU NỔI (START OVERLAY)", "#2563EB");
        btnStart.setOnClickListener(v -> checkPermissionAndStart());
        mainLayout.addView(btnStart);

        // Button: Stop Service
        Button btnStop = createButton("🛑 TẮT MENU NỔI (STOP OVERLAY)", "#DC2626");
        btnStop.setOnClickListener(v -> {
            Intent intent = new Intent(this, FloatingModMenuService.class);
            stopService(intent);
            statusTextView.setText("Trạng thái: Đã dừng Menu Nổi");
            Toast.makeText(this, "Đã dừng Menu Nổi!", Toast.LENGTH_SHORT).show();
        });
        mainLayout.addView(btnStop);

        // Guide Instructions
        TextView guideTv = new TextView(this);
        guideTv.setText("\n📌 HƯỚNG DẪN QUẢN TRỊ ONLINE VĨNH VIỄN:\n" +
                "1. Bạn KHÔNG CẦN biên dịch lại file APK này khi muốn thêm nút bấm mới.\n" +
                "2. Chỉ cần chỉnh sửa file JSON trên Server (GitHub / Vercel / VPS).\n" +
                "3. Bấm nút 'TẢI LẠI ONLINE' trên màn hình game để nạp nút bấm mới ngay!");
        guideTv.setTextColor(Color.parseColor("#CBD5E1"));
        guideTv.setTextSize(12);
        guideTv.setPadding(0, 20, 0, 20);
        mainLayout.addView(guideTv);

        scrollView.addView(mainLayout);
        setContentView(scrollView);
    }

    private void checkPermissionAndStart() {
        String inputUrl = urlEditText.getText().toString().trim();
        if (!inputUrl.isEmpty()) {
            SharedPreferences prefs = getSharedPreferences(FloatingModMenuService.PREF_NAME, MODE_PRIVATE);
            prefs.edit().putString(FloatingModMenuService.PREF_KEY_URL, inputUrl).apply();
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && !Settings.canDrawOverlays(this)) {
            Toast.makeText(this, "Hãy cấp quyền Cửa sổ nổi (Overlay Permission) cho ứng dụng!", Toast.LENGTH_LONG).show();
            Intent intent = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION, Uri.parse("package:" + getPackageName()));
            startActivityForResult(intent, OVERLAY_PERMISSION_REQ_CODE);
        } else {
            startFloatingService();
        }
    }

    private void startFloatingService() {
        String inputUrl = urlEditText.getText().toString().trim();
        Intent intent = new Intent(this, FloatingModMenuService.class);
        if (!inputUrl.isEmpty()) {
            intent.putExtra("SERVER_URL", inputUrl);
        }
        startService(intent);
        statusTextView.setText("Trạng thái: MENU NỔI ĐANG CHẠY 🔥");
        Toast.makeText(this, "Đã kích hoạt Menu Nổi Online!", Toast.LENGTH_SHORT).show();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == OVERLAY_PERMISSION_REQ_CODE) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && Settings.canDrawOverlays(this)) {
                startFloatingService();
            } else {
                Toast.makeText(this, "Cần cấp quyền Cửa sổ nổi để hiển thị Menu Mod!", Toast.LENGTH_SHORT).show();
            }
        }
    }

    private Button createButton(String text, String colorHex) {
        Button btn = new Button(this);
        btn.setText(text);
        btn.setTextColor(Color.WHITE);
        btn.setTextSize(14);

        GradientDrawable shape = new GradientDrawable();
        shape.setColor(Color.parseColor(colorHex));
        shape.setCornerRadius(16f);
        btn.setBackground(shape);

        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        );
        lp.setMargins(0, 15, 0, 15);
        btn.setLayoutParams(lp);
        return btn;
    }
}
