package com.gogs.ultimatedumper;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.DataOutputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;

public class FloatingModMenuService extends Service {

    // Native ImGui C++ Library Loader
    static {
        try {
            System.loadLibrary("imguimod");
        } catch (Throwable ignored) {}
    }

    public native String nativeScanZones(String filter);
    public native String nativeTeleportZ(float targetZ, float newZ);

    public static final String DEFAULT_CONFIG_URL = "https://raw.githubusercontent.com/Huyhub1/Mod-menu-/main/menu_config.json";
    public static final String PREF_NAME = "ARK_MOD_MENU_PREFS";
    public static final String PREF_KEY_URL = "SERVER_CONFIG_URL";

    private WindowManager windowManager;
    private View expandedView;
    private View minimizedView;
    private WindowManager.LayoutParams expandedParams;
    private WindowManager.LayoutParams minimizedParams;

    private LinearLayout containerLayout;
    private LinearLayout tabsLayout;
    private LinearLayout itemsContainerLayout;
    private TextView titleTextView;
    private TextView noticeTextView;
    private TextView logTextView;
    private ScrollView logScrollView;

    private Handler mainHandler;
    private boolean isMinimized = false;
    private JSONObject currentConfigJson = null;
    private String activeConfigUrl = DEFAULT_CONFIG_URL;

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && intent.hasExtra("SERVER_URL")) {
            String customUrl = intent.getStringExtra("SERVER_URL");
            if (customUrl != null && !customUrl.trim().isEmpty()) {
                activeConfigUrl = customUrl.trim();
            }
        } else {
            SharedPreferences prefs = getSharedPreferences(PREF_NAME, MODE_PRIVATE);
            activeConfigUrl = prefs.getString(PREF_KEY_URL, DEFAULT_CONFIG_URL);
        }
        fetchOnlineConfig(activeConfigUrl);
        return START_STICKY;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        windowManager = (WindowManager) getSystemService(WINDOW_SERVICE);
        mainHandler = new Handler(Looper.getMainLooper());

        SharedPreferences prefs = getSharedPreferences(PREF_NAME, MODE_PRIVATE);
        activeConfigUrl = prefs.getString(PREF_KEY_URL, DEFAULT_CONFIG_URL);

        createMinimizedBadge();
        createExpandedMenu();

        fetchOnlineConfig(activeConfigUrl);
    }

    // ================================================================
    // MINIMIZED FLOATING BADGE (Icon bong bóng nhỏ khi ẩn)
    // ================================================================
    private void createMinimizedBadge() {
        Button badgeBtn = new Button(this);
        badgeBtn.setText("ARK\nMOD");
        badgeBtn.setTextColor(Color.WHITE);
        badgeBtn.setTextSize(11);

        GradientDrawable shape = new GradientDrawable();
        shape.setShape(GradientDrawable.OVAL);
        shape.setColor(Color.parseColor("#1976D2"));
        shape.setStroke(4, Color.parseColor("#60A5FA"));
        badgeBtn.setBackground(shape);

        int layoutType = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                ? WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
                : WindowManager.LayoutParams.TYPE_PHONE;

        minimizedParams = new WindowManager.LayoutParams(
                140, 140,
                layoutType,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
                PixelFormat.TRANSLUCENT
        );
        minimizedParams.gravity = Gravity.TOP | Gravity.LEFT;
        minimizedParams.x = 20;
        minimizedParams.y = 200;

        badgeBtn.setOnClickListener(v -> toggleMinimize(false));

        badgeBtn.setOnTouchListener(new View.OnTouchListener() {
            private int initialX, initialY;
            private float initialTouchX, initialTouchY;

            @Override
            public boolean onTouch(View v, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        initialX = minimizedParams.x;
                        initialY = minimizedParams.y;
                        initialTouchX = event.getRawX();
                        initialTouchY = event.getRawY();
                        return true;
                    case MotionEvent.ACTION_MOVE:
                        minimizedParams.x = initialX + (int) (event.getRawX() - initialTouchX);
                        minimizedParams.y = initialY + (int) (event.getRawY() - initialTouchY);
                        windowManager.updateViewLayout(minimizedView, minimizedParams);
                        return true;
                }
                return false;
            }
        });

        minimizedView = badgeBtn;
    }

    // ================================================================
    // EXPANDED MAIN FLOATING OVERLAY WINDOW
    // ================================================================
    private void createExpandedMenu() {
        containerLayout = new LinearLayout(this);
        containerLayout.setOrientation(LinearLayout.VERTICAL);

        GradientDrawable bg = new GradientDrawable();
        bg.setColor(Color.parseColor("#EA0F172A")); // Deep dark glassmorphism (Translucent)
        bg.setCornerRadius(24f);
        bg.setStroke(3, Color.parseColor("#3B82F6"));
        containerLayout.setBackground(bg);
        containerLayout.setPadding(24, 20, 24, 20);

        // Header Title
        titleTextView = new TextView(this);
        titleTextView.setText("ARK Ultimate Online Menu v3.0");
        titleTextView.setTextColor(Color.parseColor("#60A5FA"));
        titleTextView.setTextSize(15);
        titleTextView.setGravity(Gravity.CENTER);
        titleTextView.setPadding(0, 5, 0, 5);
        containerLayout.addView(titleTextView);

        // Notice Ticker Banner
        noticeTextView = new TextView(this);
        noticeTextView.setText("⚡ Đang kết nối Server Online...");
        noticeTextView.setTextColor(Color.parseColor("#FBBF24"));
        noticeTextView.setTextSize(11);
        noticeTextView.setGravity(Gravity.CENTER);
        noticeTextView.setPadding(0, 0, 0, 10);
        containerLayout.addView(noticeTextView);

        // Top Action Bar: Sync Online + Minimize
        LinearLayout topBar = new LinearLayout(this);
        topBar.setOrientation(LinearLayout.HORIZONTAL);

        Button btnSync = createStyledButton("🔄 Tải Lại Online", "#059669");
        btnSync.setLayoutParams(new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        btnSync.setOnClickListener(v -> fetchOnlineConfig(activeConfigUrl));
        topBar.addView(btnSync);

        Button btnMin = createStyledButton("➖ Thu Nhỏ", "#DC2626");
        btnMin.setLayoutParams(new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        btnMin.setOnClickListener(v -> toggleMinimize(true));
        topBar.addView(btnMin);

        containerLayout.addView(topBar);

        // Category Tabs Bar (Horizontal Scrollable)
        HorizontalScrollView tabsScrollView = new HorizontalScrollView(this);
        tabsScrollView.setPadding(0, 10, 0, 10);
        tabsLayout = new LinearLayout(this);
        tabsLayout.setOrientation(LinearLayout.HORIZONTAL);
        tabsScrollView.addView(tabsLayout);
        containerLayout.addView(tabsScrollView);

        // Dynamic Items Container (Vertical Scrollable)
        ScrollView itemsScrollView = new ScrollView(this);
        LinearLayout.LayoutParams itemsLp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 320
        );
        itemsScrollView.setLayoutParams(itemsLp);
        itemsContainerLayout = new LinearLayout(this);
        itemsContainerLayout.setOrientation(LinearLayout.VERTICAL);
        itemsScrollView.addView(itemsContainerLayout);
        containerLayout.addView(itemsScrollView);

        // Log Console Display Output
        logScrollView = new ScrollView(this);
        LinearLayout.LayoutParams logLp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 220
        );
        logLp.setMargins(0, 10, 0, 0);
        logScrollView.setLayoutParams(logLp);

        logTextView = new TextView(this);
        logTextView.setText("Console Log Output Ready...\nSẵn sàng thực thi lệnh từ Server Online!");
        logTextView.setTextColor(Color.parseColor("#34D399"));
        logTextView.setTextSize(11);
        logTextView.setBackgroundColor(Color.parseColor("#0F172A"));
        logTextView.setPadding(12, 12, 12, 12);
        logScrollView.addView(logTextView);
        containerLayout.addView(logScrollView);

        int layoutType = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                ? WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
                : WindowManager.LayoutParams.TYPE_PHONE;

        expandedParams = new WindowManager.LayoutParams(
                720, WindowManager.LayoutParams.WRAP_CONTENT,
                layoutType,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
                PixelFormat.TRANSLUCENT
        );

        expandedParams.gravity = Gravity.TOP | Gravity.LEFT;
        expandedParams.x = 100;
        expandedParams.y = 100;

        // Make Expanded Window Draggable
        containerLayout.setOnTouchListener(new View.OnTouchListener() {
            private int initialX, initialY;
            private float initialTouchX, initialTouchY;

            @Override
            public boolean onTouch(View v, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        initialX = expandedParams.x;
                        initialY = expandedParams.y;
                        initialTouchX = event.getRawX();
                        initialTouchY = event.getRawY();
                        return true;
                    case MotionEvent.ACTION_MOVE:
                        expandedParams.x = initialX + (int) (event.getRawX() - initialTouchX);
                        expandedParams.y = initialY + (int) (event.getRawY() - initialTouchY);
                        windowManager.updateViewLayout(containerLayout, expandedParams);
                        return true;
                }
                return false;
            }
        });

        expandedView = containerLayout;
        windowManager.addView(expandedView, expandedParams);
    }

    private void toggleMinimize(boolean minimize) {
        isMinimized = minimize;
        if (minimize) {
            if (expandedView != null && expandedView.isAttachedToWindow()) {
                windowManager.removeView(expandedView);
            }
            if (minimizedView != null && !minimizedView.isAttachedToWindow()) {
                windowManager.addView(minimizedView, minimizedParams);
            }
        } else {
            if (minimizedView != null && minimizedView.isAttachedToWindow()) {
                windowManager.removeView(minimizedView);
            }
            if (expandedView != null && !expandedView.isAttachedToWindow()) {
                windowManager.addView(expandedView, expandedParams);
            }
        }
    }

    // ================================================================
    // FETCH ONLINE CONFIG FROM SERVER (HTTP JSON)
    // ================================================================
    private void fetchOnlineConfig(String urlStr) {
        appendLog("[+] Đang tải cấu hình menu từ Server Online...");
        new Thread(() -> {
            try {
                URL url = new URL(urlStr);
                HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                conn.setConnectTimeout(6000);
                conn.setReadTimeout(6000);
                conn.setRequestMethod("GET");

                int code = conn.getResponseCode();
                if (code == 200) {
                    BufferedReader br = new BufferedReader(new InputStreamReader(conn.getInputStream()));
                    StringBuilder sb = new StringBuilder();
                    String line;
                    while ((line = br.readLine()) != null) {
                        sb.append(line).append("\n");
                    }
                    br.close();

                    String jsonResult = sb.toString();
                    JSONObject json = new JSONObject(jsonResult);
                    mainHandler.post(() -> renderDynamicMenu(json, true));
                } else {
                    mainHandler.post(() -> {
                        appendLog("[-] Server trả về lỗi HTTP: " + code + ". Nạp cấu hình mặc định...");
                        loadFallbackConfig();
                    });
                }
            } catch (Exception e) {
                mainHandler.post(() -> {
                    appendLog("[-] Lỗi kết nối Server: " + e.getMessage() + ". Nạp cấu hình Offline...");
                    loadFallbackConfig();
                });
            }
        }).start();
    }

    private void loadFallbackConfig() {
        try {
            String defaultJsonStr = "{\n" +
                    "  \"menu_title\": \"ARK Ultimate Cloud Menu (Offline)\",\n" +
                    "  \"announcement\": \"⚠️ Chế độ Offline. Nhấn 'Tải Lại Online' để kết nối Server!\",\n" +
                    "  \"categories\": [\n" +
                    "    {\n" +
                    "      \"cat_name\": \"📍 Dump Zone\",\n" +
                    "      \"items\": [\n" +
                    "        {\"label\": \"Quét Zone Spawn (Native C++)\", \"action_type\": \"native_scan_zones\", \"payload\": \"\"},\n" +
                    "        {\"label\": \"Quét Chuỗi RAM DinoSpawnEntries_\", \"action_type\": \"root_cmd\", \"payload\": \"PIDS=$(pidof com.studiowildcard.wardrumstudios.ark || pgrep -f ark || pgrep -f wildcard); if [ -n \\\"$PIDS\\\" ]; then for PID in $PIDS; do echo \\\"[+] Đang quét RAM PID: $PID...\\\"; grep -a -o 'DinoSpawnEntries_[A-Za-z0-9_]*' /proc/$PID/mem 2>/dev/null | sort -u; done; else echo '[-] Không tìm thấy PID game!'; fi\"}\n" +
                    "      ]\n" +
                    "    },\n" +
                    "    {\n" +
                    "      \"cat_name\": \"🎯 Aim & Target\",\n" +
                    "      \"items\": [\n" +
                    "        {\"label\": \"Quét APrimalDinoCharacter\", \"action_type\": \"root_cmd\", \"payload\": \"PIDS=$(pidof com.studiowildcard.wardrumstudios.ark || pgrep -f ark || pgrep -f wildcard); if [ -n \\\"$PIDS\\\" ]; then for PID in $PIDS; do grep -a -o 'APrimalDinoCharacter_[A-Za-z0-9_]*' /proc/$PID/mem 2>/dev/null | head -n 15; done; else echo '[-] Không tìm thấy PID game!'; fi\"}\n" +
                    "      ]\n" +
                    "    }\n" +
                    "  ]\n" +
                    "}";
            renderDynamicMenu(new JSONObject(defaultJsonStr), false);
        } catch (Exception ignored) {}
    }

    // ================================================================
    // DYNAMIC UI RENDERER FROM ONLINE JSON
    // ================================================================
    private void renderDynamicMenu(JSONObject json, boolean isOnline) {
        currentConfigJson = json;
        try {
            String title = json.optString("menu_title", "ARK Cloud Menu");
            String notice = json.optString("announcement", "Server Connected!");
            titleTextView.setText(title);
            noticeTextView.setText(notice);

            appendLog(isOnline ? "[✔] ĐÃ KẾT NỐI SERVER ONLINE THÀNH CÔNG!" : "[!] Đã nạp cấu hình Offline.");

            tabsLayout.removeAllViews();
            itemsContainerLayout.removeAllViews();

            JSONArray categories = json.optJSONArray("categories");
            if (categories != null && categories.length() > 0) {
                for (int i = 0; i < categories.length(); i++) {
                    JSONObject cat = categories.getJSONObject(i);
                    String catName = cat.optString("cat_name", "Tab " + (i + 1));
                    final int catIndex = i;

                    Button tabBtn = createStyledButton(catName, i == 0 ? "#2563EB" : "#374151");
                    tabBtn.setOnClickListener(v -> selectCategoryTab(catIndex));
                    tabsLayout.addView(tabBtn);
                }
                selectCategoryTab(0);
            }

        } catch (Exception e) {
            appendLog("[-] Lỗi render menu UI: " + e.getMessage());
        }
    }

    private void selectCategoryTab(int index) {
        if (currentConfigJson == null) return;
        try {
            for (int i = 0; i < tabsLayout.getChildCount(); i++) {
                View child = tabsLayout.getChildAt(i);
                if (child instanceof Button) {
                    child.setBackgroundColor(Color.parseColor(i == index ? "#2563EB" : "#374151"));
                }
            }

            itemsContainerLayout.removeAllViews();
            JSONArray categories = currentConfigJson.optJSONArray("categories");
            if (categories != null && index < categories.length()) {
                JSONObject cat = categories.getJSONObject(index);
                JSONArray items = cat.optJSONArray("items");

                if (items != null) {
                    for (int j = 0; j < items.length(); j++) {
                        JSONObject item = items.getJSONObject(j);
                        String type = item.optString("type", "button");
                        String label = item.optString("label", "Action");
                        String payload = item.optString("payload", "");
                        String actionType = item.optString("action_type", "root_cmd");

                        if (type.equals("button")) {
                            Button itemBtn = createStyledButton(label, "#1D4ED8");
                            itemBtn.setOnClickListener(v -> executeMenuAction(actionType, payload, ""));
                            itemsContainerLayout.addView(itemBtn);

                        } else if (type.equals("input_button")) {
                            LinearLayout inputRow = new LinearLayout(this);
                            inputRow.setOrientation(LinearLayout.HORIZONTAL);

                            EditText input = new EditText(this);
                            input.setHint(label);
                            input.setText(item.optString("default_val", ""));
                            input.setTextColor(Color.WHITE);
                            input.setHintTextColor(Color.GRAY);
                            input.setBackgroundColor(Color.parseColor("#1E293B"));
                            input.setPadding(10, 10, 10, 10);
                            input.setLayoutParams(new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 2f));
                            inputRow.addView(input);

                            Button actionBtn = createStyledButton(item.optString("btn_label", "Gửi"), "#059669");
                            actionBtn.setLayoutParams(new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
                            actionBtn.setOnClickListener(v -> executeMenuAction(actionType, payload, input.getText().toString()));
                            inputRow.addView(actionBtn);

                            itemsContainerLayout.addView(inputRow);

                        } else if (type.equals("toggle")) {
                            Button toggleBtn = createStyledButton("[OFF] " + label, "#475569");
                            final boolean[] state = {false};
                            toggleBtn.setOnClickListener(v -> {
                                state[0] = !state[0];
                                toggleBtn.setText((state[0] ? "[ON] " : "[OFF] ") + label);
                                toggleBtn.setBackgroundColor(Color.parseColor(state[0] ? "#16A34A" : "#475569"));
                                String cmd = state[0] ? item.optString("on_cmd", "") : item.optString("off_cmd", "");
                                executeMenuAction("root_cmd", cmd, "");
                            });
                            itemsContainerLayout.addView(toggleBtn);
                        }
                    }
                }
            }

        } catch (Exception e) {
            appendLog("[-] Lỗi chuyển tab: " + e.getMessage());
        }
    }

    // ================================================================
    // EXECUTION ENGINE FOR ROOT SHELL / NATIVE C++ IMGUI COMMANDS
    // ================================================================
    private void executeMenuAction(String actionType, String payload, String inputVal) {
        appendLog("[⚡] Thực thi: " + actionType + " | Input: " + inputVal);

        new Thread(() -> {
            try {
                if (actionType.equals("native_scan_zones")) {
                    try {
                        String nativeRes = nativeScanZones(inputVal);
                        mainHandler.post(() -> appendLog(nativeRes));
                        return;
                    } catch (Throwable t) {
                        appendLog("[-] Lỗi Native: " + t.getMessage() + ". Đang chuyển sang Root Shell...");
                    }
                } else if (actionType.equals("native_teleport_z")) {
                    try {
                        float newZ = Float.parseFloat(inputVal.isEmpty() ? "2000" : inputVal);
                        String nativeRes = nativeTeleportZ(200.0f, newZ);
                        mainHandler.post(() -> appendLog(nativeRes));
                        return;
                    } catch (Throwable t) {
                        appendLog("[-] Lỗi Teleport: " + t.getMessage());
                    }
                }

                String cmdToRun = payload;
                if (actionType.equals("root_cmd_input")) {
                    cmdToRun = inputVal;
                } else if (actionType.equals("teleport_z")) {
                    cmdToRun = "PIDS=$(pidof com.studiowildcard.wardrumstudios.ark || pgrep -f ark); echo \"[TELEPORT Z] Ghi Z = " + inputVal + " vào RAM (PIDs: $PIDS)\"";
                }

                if (cmdToRun == null || cmdToRun.isEmpty()) {
                    mainHandler.post(() -> appendLog("[-] Không có câu lệnh để thực thi."));
                    return;
                }

                Process process = Runtime.getRuntime().exec("su");
                DataOutputStream os = new DataOutputStream(process.getOutputStream());
                BufferedReader stdoutReader = new BufferedReader(new InputStreamReader(process.getInputStream()));
                BufferedReader stderrReader = new BufferedReader(new InputStreamReader(process.getErrorStream()));

                os.writeBytes(cmdToRun + "\n");
                os.writeBytes("exit\n");
                os.flush();

                StringBuilder output = new StringBuilder();
                String line;
                while ((line = stdoutReader.readLine()) != null) {
                    output.append(line).append("\n");
                }
                while ((line = stderrReader.readLine()) != null) {
                    output.append("[ERR] ").append(line).append("\n");
                }
                process.waitFor();

                String result = output.toString().trim();
                mainHandler.post(() -> {
                    if (!result.isEmpty()) {
                        appendLog(result);
                    } else {
                        appendLog("[✔] Lệnh đã thực thi thành công.");
                    }
                });

            } catch (Exception e) {
                mainHandler.post(() -> appendLog("[-] Lỗi Root Exec: " + e.getMessage()));
            }
        }).start();
    }

    private void appendLog(String text) {
        if (logTextView != null) {
            String current = logTextView.getText().toString();
            String updated = text + "\n---\n" + current;
            if (updated.length() > 4000) updated = updated.substring(0, 4000);
            logTextView.setText(updated);
            logScrollView.post(() -> logScrollView.fullScroll(ScrollView.FOCUS_UP));
        }
    }

    private Button createStyledButton(String text, String colorHex) {
        Button btn = new Button(this);
        btn.setText(text);
        btn.setTextColor(Color.WHITE);
        btn.setTextSize(12);

        GradientDrawable shape = new GradientDrawable();
        shape.setColor(Color.parseColor(colorHex));
        shape.setCornerRadius(14f);
        btn.setBackground(shape);

        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        );
        lp.setMargins(4, 6, 4, 6);
        btn.setLayoutParams(lp);
        return btn;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (expandedView != null && expandedView.isAttachedToWindow()) windowManager.removeView(expandedView);
        if (minimizedView != null && minimizedView.isAttachedToWindow()) windowManager.removeView(minimizedView);
    }
}
