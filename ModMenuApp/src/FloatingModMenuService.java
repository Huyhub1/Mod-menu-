package com.gogs.ultimatedumper;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.Typeface;
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
    // MINIMIZED FLOATING BADGE (Icon bong bóng Neon nhỏ gọn)
    // ================================================================
    private void createMinimizedBadge() {
        Button badgeBtn = new Button(this);
        badgeBtn.setText("ARK\nMOD");
        badgeBtn.setTextColor(Color.WHITE);
        badgeBtn.setTextSize(10);
        badgeBtn.setTypeface(Typeface.DEFAULT_BOLD);

        GradientDrawable shape = new GradientDrawable();
        shape.setShape(GradientDrawable.OVAL);
        shape.setColor(Color.parseColor("#0F172A"));
        shape.setStroke(4, Color.parseColor("#38BDF8"));
        badgeBtn.setBackground(shape);

        int layoutType = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                ? WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
                : WindowManager.LayoutParams.TYPE_PHONE;

        minimizedParams = new WindowManager.LayoutParams(
                130, 130,
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
    // EXPANDED MAIN FLOATING OVERLAY WINDOW (Modern Glassmorphism UI)
    // ================================================================
    private void createExpandedMenu() {
        containerLayout = new LinearLayout(this);
        containerLayout.setOrientation(LinearLayout.VERTICAL);

        GradientDrawable bg = new GradientDrawable();
        bg.setColor(Color.parseColor("#F00B0F19")); // Deep slate dark glassmorphism
        bg.setCornerRadius(20f);
        bg.setStroke(2, Color.parseColor("#1E293B"));
        containerLayout.setBackground(bg);
        containerLayout.setPadding(18, 16, 18, 16);

        // Header Title Bar with Status Pill
        LinearLayout headerBar = new LinearLayout(this);
        headerBar.setOrientation(LinearLayout.HORIZONTAL);
        headerBar.setGravity(Gravity.CENTER_VERTICAL);
        headerBar.setPadding(6, 4, 6, 8);

        titleTextView = new TextView(this);
        titleTextView.setText("⚡ ARK ULTIMATE MOD");
        titleTextView.setTextColor(Color.parseColor("#38BDF8"));
        titleTextView.setTextSize(14);
        titleTextView.setTypeface(Typeface.DEFAULT_BOLD);
        titleTextView.setLayoutParams(new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        headerBar.addView(titleTextView);

        TextView statusPill = new TextView(this);
        statusPill.setText("🟢 ONLINE");
        statusPill.setTextColor(Color.parseColor("#4ADE80"));
        statusPill.setTextSize(10);
        statusPill.setTypeface(Typeface.DEFAULT_BOLD);
        statusPill.setPadding(12, 4, 12, 4);
        GradientDrawable pillBg = new GradientDrawable();
        pillBg.setColor(Color.parseColor("#14532D"));
        pillBg.setCornerRadius(10f);
        statusPill.setBackground(pillBg);
        headerBar.addView(statusPill);

        containerLayout.addView(headerBar);

        // Sub Notice Ticker
        noticeTextView = new TextView(this);
        noticeTextView.setText("Cloud Server Connected");
        noticeTextView.setTextColor(Color.parseColor("#94A3B8"));
        noticeTextView.setTextSize(10);
        noticeTextView.setPadding(6, 0, 6, 8);
        containerLayout.addView(noticeTextView);

        // Compact Top Control Action Buttons
        LinearLayout topBar = new LinearLayout(this);
        topBar.setOrientation(LinearLayout.HORIZONTAL);

        Button btnSync = createStyledButton("🔄 Sync Online", "#059669", 11);
        btnSync.setLayoutParams(new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        btnSync.setOnClickListener(v -> fetchOnlineConfig(activeConfigUrl));
        topBar.addView(btnSync);

        Button btnMin = createStyledButton("➖ Thu Nhỏ", "#E11D48", 11);
        btnMin.setLayoutParams(new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        btnMin.setOnClickListener(v -> toggleMinimize(true));
        topBar.addView(btnMin);

        containerLayout.addView(topBar);

        // Category Tabs Bar (Pill Buttons)
        HorizontalScrollView tabsScrollView = new HorizontalScrollView(this);
        tabsScrollView.setPadding(0, 6, 0, 6);
        tabsScrollView.setHorizontalScrollBarEnabled(false);
        tabsLayout = new LinearLayout(this);
        tabsLayout.setOrientation(LinearLayout.HORIZONTAL);
        tabsScrollView.addView(tabsLayout);
        containerLayout.addView(tabsScrollView);

        // Dynamic Items Container (Scrollable)
        ScrollView itemsScrollView = new ScrollView(this);
        LinearLayout.LayoutParams itemsLp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 260
        );
        itemsScrollView.setLayoutParams(itemsLp);
        itemsContainerLayout = new LinearLayout(this);
        itemsContainerLayout.setOrientation(LinearLayout.VERTICAL);
        itemsScrollView.addView(itemsContainerLayout);
        containerLayout.addView(itemsScrollView);

        // Sleek Terminal Console Display Output
        logScrollView = new ScrollView(this);
        LinearLayout.LayoutParams logLp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 180
        );
        logLp.setMargins(0, 8, 0, 0);
        logScrollView.setLayoutParams(logLp);

        logTextView = new TextView(this);
        logTextView.setText("> System Ready. Sẵn sàng thực thi lệnh!");
        logTextView.setTextColor(Color.parseColor("#4ADE80"));
        logTextView.setTextSize(10);
        logTextView.setTypeface(Typeface.MONOSPACE);
        
        GradientDrawable logBg = new GradientDrawable();
        logBg.setColor(Color.parseColor("#020617"));
        logBg.setCornerRadius(12f);
        logBg.setStroke(1, Color.parseColor("#1E293B"));
        logTextView.setBackground(logBg);
        logTextView.setPadding(10, 10, 10, 10);
        logScrollView.addView(logTextView);
        containerLayout.addView(logScrollView);

        int layoutType = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                ? WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
                : WindowManager.LayoutParams.TYPE_PHONE;

        expandedParams = new WindowManager.LayoutParams(
                660, WindowManager.LayoutParams.WRAP_CONTENT,
                layoutType,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
                PixelFormat.TRANSLUCENT
        );

        expandedParams.gravity = Gravity.TOP | Gravity.LEFT;
        expandedParams.x = 80;
        expandedParams.y = 80;

        // Draggable Overlay Setup
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
        appendLog("[+] Đang tải cấu hình Online...");
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
                        appendLog("[-] HTTP Error: " + code + ". Nạp cấu hình Offline...");
                        loadFallbackConfig();
                    });
                }
            } catch (Exception e) {
                mainHandler.post(() -> {
                    appendLog("[-] Lỗi Server: " + e.getMessage() + ". Nạp cấu hình Offline...");
                    loadFallbackConfig();
                });
            }
        }).start();
    }

    private void loadFallbackConfig() {
        try {
            String defaultJsonStr = "{\n" +
                    "  \"menu_title\": \"⚡ ARK MOD ENGINE\",\n" +
                    "  \"announcement\": \"⚠️ Chế độ Offline Connected\",\n" +
                    "  \"categories\": [\n" +
                    "    {\n" +
                    "      \"cat_name\": \"🧪 Script GitHub\",\n" +
                    "      \"items\": [\n" +
                    "        {\"label\": \"▶️ Test Zones Script\", \"action_type\": \"run_url_script\", \"payload\": \"https://raw.githubusercontent.com/Huyhub1/Mod-menu-/main/scripts/test_zones.sh\"}\n" +
                    "      ]\n" +
                    "    },\n" +
                    "    {\n" +
                    "      \"cat_name\": \"📍 Spawn Zones\",\n" +
                    "      \"items\": [\n" +
                    "        {\"label\": \"Quét Spawn Zones (Native)\", \"action_type\": \"native_scan_zones\", \"payload\": \"\"}\n" +
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
            String title = json.optString("menu_title", "⚡ ARK ULTIMATE MOD");
            String notice = json.optString("announcement", "Cloud Server Connected");
            titleTextView.setText(title);
            noticeTextView.setText(notice);

            appendLog(isOnline ? "[✔] ĐÃ ĐỒNG BỘ CONFIG ONLINE TỪ GITHUB!" : "[!] Đã nạp cấu hình Offline.");

            tabsLayout.removeAllViews();
            itemsContainerLayout.removeAllViews();

            JSONArray categories = json.optJSONArray("categories");
            if (categories != null && categories.length() > 0) {
                for (int i = 0; i < categories.length(); i++) {
                    JSONObject cat = categories.getJSONObject(i);
                    String catName = cat.optString("cat_name", "Tab " + (i + 1));
                    final int catIndex = i;

                    Button tabBtn = createTabPillButton(catName, i == 0);
                    tabBtn.setOnClickListener(v -> selectCategoryTab(catIndex));
                    tabsLayout.addView(tabBtn);
                }
                selectCategoryTab(0);
            }

        } catch (Exception e) {
            appendLog("[-] Lỗi render UI: " + e.getMessage());
        }
    }

    private void selectCategoryTab(int index) {
        if (currentConfigJson == null) return;
        try {
            for (int i = 0; i < tabsLayout.getChildCount(); i++) {
                View child = tabsLayout.getChildAt(i);
                if (child instanceof Button) {
                    Button b = (Button) child;
                    GradientDrawable shape = new GradientDrawable();
                    if (i == index) {
                        shape.setColor(Color.parseColor("#2563EB"));
                        shape.setCornerRadius(16f);
                        b.setTextColor(Color.WHITE);
                    } else {
                        shape.setColor(Color.parseColor("#1E293B"));
                        shape.setCornerRadius(16f);
                        b.setTextColor(Color.parseColor("#94A3B8"));
                    }
                    b.setBackground(shape);
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
                            Button itemBtn = createStyledButton(label, "#1D4ED8", 11);
                            itemBtn.setOnClickListener(v -> executeMenuAction(actionType, payload, ""));
                            itemsContainerLayout.addView(itemBtn);

                        } else if (type.equals("input_button")) {
                            LinearLayout inputRow = new LinearLayout(this);
                            inputRow.setOrientation(LinearLayout.HORIZONTAL);
                            inputRow.setPadding(0, 4, 0, 4);

                            EditText input = new EditText(this);
                            input.setHint(label);
                            input.setText(item.optString("default_val", ""));
                            input.setTextColor(Color.WHITE);
                            input.setHintTextColor(Color.GRAY);
                            input.setTextSize(11);
                            
                            GradientDrawable inputBg = new GradientDrawable();
                            inputBg.setColor(Color.parseColor("#0F172A"));
                            inputBg.setCornerRadius(10f);
                            inputBg.setStroke(1, Color.parseColor("#334155"));
                            input.setBackground(inputBg);
                            input.setPadding(14, 10, 14, 10);
                            input.setLayoutParams(new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 2f));
                            inputRow.addView(input);

                            Button actionBtn = createStyledButton(item.optString("btn_label", "Gửi"), "#059669", 11);
                            actionBtn.setLayoutParams(new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
                            actionBtn.setOnClickListener(v -> executeMenuAction(actionType, payload, input.getText().toString()));
                            inputRow.addView(actionBtn);

                            itemsContainerLayout.addView(inputRow);

                        } else if (type.equals("toggle")) {
                            Button toggleBtn = createStyledButton("[OFF] " + label, "#334155", 11);
                            final boolean[] state = {false};
                            toggleBtn.setOnClickListener(v -> {
                                state[0] = !state[0];
                                toggleBtn.setText((state[0] ? "[ON] " : "[OFF] ") + label);
                                toggleBtn.setBackgroundColor(Color.parseColor(state[0] ? "#16A34A" : "#334155"));
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
    // EXECUTION ENGINE FOR ROOT SHELL / SCRIPT URL / NATIVE COMMANDS
    // ================================================================
    private void executeMenuAction(String actionType, String payload, String inputVal) {
        appendLog("[⚡] Run: " + actionType + " | " + inputVal);

        new Thread(() -> {
            try {
                if (actionType.equals("native_scan_zones")) {
                    try {
                        String nativeRes = nativeScanZones(inputVal);
                        mainHandler.post(() -> appendLog(nativeRes));
                        return;
                    } catch (Throwable t) {
                        appendLog("[-] Lỗi Native: " + t.getMessage());
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
                if (actionType.equals("run_url_script")) {
                    String scriptUrl = inputVal.isEmpty() ? payload : inputVal;
                    cmdToRun = "echo '[📥] Đang nạp Script từ GitHub: " + scriptUrl + "' && curl -s -m 8 '" + scriptUrl + "' | sh 2>&1";
                } else if (actionType.equals("root_cmd_input")) {
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
                        appendLog("[✔] Lệnh đã thực thi xong.");
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
            if (updated.length() > 3500) updated = updated.substring(0, 3500);
            logTextView.setText(updated);
            logScrollView.post(() -> logScrollView.fullScroll(ScrollView.FOCUS_UP));
        }
    }

    private Button createTabPillButton(String text, boolean isActive) {
        Button btn = new Button(this);
        btn.setText(text);
        btn.setTextSize(11);
        btn.setTypeface(Typeface.DEFAULT_BOLD);

        GradientDrawable shape = new GradientDrawable();
        if (isActive) {
            shape.setColor(Color.parseColor("#2563EB"));
            btn.setTextColor(Color.WHITE);
        } else {
            shape.setColor(Color.parseColor("#1E293B"));
            btn.setTextColor(Color.parseColor("#94A3B8"));
        }
        shape.setCornerRadius(16f);
        btn.setBackground(shape);

        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        );
        lp.setMargins(4, 2, 4, 2);
        btn.setPadding(20, 10, 20, 10);
        btn.setLayoutParams(lp);
        return btn;
    }

    private Button createStyledButton(String text, String colorHex, int textSizeDp) {
        Button btn = new Button(this);
        btn.setText(text);
        btn.setTextColor(Color.WHITE);
        btn.setTextSize(textSizeDp);
        btn.setTypeface(Typeface.DEFAULT_BOLD);

        GradientDrawable shape = new GradientDrawable();
        shape.setColor(Color.parseColor(colorHex));
        shape.setCornerRadius(12f);
        btn.setBackground(shape);

        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        );
        lp.setMargins(4, 4, 4, 4);
        btn.setPadding(10, 8, 10, 8);
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
