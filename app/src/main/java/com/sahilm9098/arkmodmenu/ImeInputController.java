package com.sahilm9098.arkmodmenu;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.text.InputType;
import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

final class ImeInputController {
    private static final String TAG = "ImeInputController";
    private static final int MODE_NONE = 0;
    private static final int MODE_SINGLE_LINE = 1;
    private static final int MODE_MULTI_LINE = 2;
    private static final int COLOR_PANEL = 0xfffbfbfd;
    private static final int COLOR_HEADER = 0xffffffff;
    private static final int COLOR_EDITOR = 0xfff4f6f8;
    private static final int COLOR_BORDER = 0x1f000000;
    private static final int COLOR_EDITOR_BORDER = 0xff111111;
    private static final int COLOR_DIVIDER = 0x14000000;
    private static final int COLOR_TEXT = 0xff16181d;
    private static final int COLOR_MUTED_TEXT = 0xff646a73;
    private static final int COLOR_ACTION = 0xff2f6fed;

    private static Activity activity;
    private static WindowManager windowManager;
    private static View panel;
    private static EditText editor;
    private static int activeId;
    private static int mode;

    private ImeInputController() {}

    static void attach(Activity targetActivity, WindowManager targetWindowManager) {
        activity = targetActivity;
        windowManager = targetWindowManager;
    }

    static void update(Activity currentActivity) {
        if (currentActivity == null || windowManager == null) {
            dismissLocal(false);
            return;
        }

        if (activity != currentActivity) {
            activity = currentActivity;
        }

        String[] state = GLES3JNIView.getKeyboardState();
        int nextMode = parseInt(state, 0, MODE_NONE);
        int nextActiveId = parseInt(state, 1, 0);
        String text = state != null && state.length > 2 && state[2] != null ? state[2] : "";

        if (nextMode == MODE_NONE || nextActiveId == 0) {
            dismissLocal(false);
            return;
        }

        if (panel == null || activeId != nextActiveId || mode != nextMode) {
            showEditor(nextMode, nextActiveId, text);
        }
    }

    static void stop() {
        dismissLocal(true);
        activity = null;
        windowManager = null;
    }

    private static void showEditor(int nextMode, int nextActiveId, String initialText) {
        dismissLocal(false);
        if (activity == null || windowManager == null) {
            return;
        }

        mode = nextMode;
        activeId = nextActiveId;
        editor = createEditor(nextMode, initialText);
        panel = createPanel(nextMode, editor);

        try {
            windowManager.addView(panel, createLayoutParams(nextMode));
        } catch (RuntimeException exception) {
            Log.w(TAG, "failed to show IME panel", exception);
            panel = null;
            editor = null;
            return;
        }

        editor.requestFocus();
        editor.post(new Runnable() {
            @Override
            public void run() {
                InputMethodManager inputManager = (InputMethodManager)
                        activity.getSystemService(Context.INPUT_METHOD_SERVICE);
                if (inputManager != null) {
                    inputManager.showSoftInput(editor, InputMethodManager.SHOW_IMPLICIT);
                }
            }
        });
    }

    private static EditText createEditor(int editorMode, String initialText) {
        final boolean multiline = editorMode == MODE_MULTI_LINE;
        EditText editText = new EditText(activity);
        editText.setText(initialText);
        editText.setTextColor(COLOR_TEXT);
        editText.setHintTextColor(0xff868c96);
        editText.setTextSize(multiline ? 16.0f : 18.0f);
        editText.setSelectAllOnFocus(false);
        editText.setSingleLine(!multiline);
        editText.setGravity(multiline ? Gravity.TOP | Gravity.LEFT : Gravity.CENTER_VERTICAL);
        editText.setMinHeight(multiline ? dp(160) : dp(52));
        editText.setInputType(createInputType(multiline));
        editText.setImeOptions(multiline
                ? EditorInfo.IME_FLAG_NO_EXTRACT_UI
                : EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_EXTRACT_UI);
        editText.setBackground(makeRoundedDrawable(COLOR_EDITOR, dp(10), COLOR_EDITOR_BORDER, 2));
        editText.setSelection(editText.getText().length());

        editText.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (!multiline && (actionId == EditorInfo.IME_ACTION_DONE
                        || isEnterUp(event))) {
                    commit();
                    return true;
                }
                return false;
            }
        });

        editText.setOnKeyListener(new View.OnKeyListener() {
            @Override
            public boolean onKey(View v, int keyCode, KeyEvent event) {
                if (keyCode == KeyEvent.KEYCODE_BACK
                        && event.getAction() == KeyEvent.ACTION_UP) {
                    cancel();
                    return true;
                }
                return false;
            }
        });

        return editText;
    }

    private static View createPanel(int editorMode, EditText editText) {
        final boolean multiline = editorMode == MODE_MULTI_LINE;
        LinearLayout root = new LinearLayout(activity);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackground(makeRoundedDrawable(COLOR_PANEL, multiline ? 0 : dp(14), COLOR_BORDER, 1));
        root.setPadding(multiline ? 0 : dp(12), 0, multiline ? 0 : dp(12), multiline ? dp(12) : dp(12));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            root.setElevation(dp(10));
        }

        LinearLayout buttons = createButtonHeader();
        root.addView(buttons, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(50)));

        View divider = new View(activity);
        divider.setBackgroundColor(COLOR_DIVIDER);
        root.addView(divider, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 1));

        editText.setPadding(dp(14), dp(10), dp(14), dp(10));
        LinearLayout.LayoutParams editorParams = multiline
                ? new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, 0, 1.0f)
                : new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        editorParams.topMargin = dp(12);
        root.addView(editText, editorParams);
        return root;
    }

    private static LinearLayout createButtonHeader() {
        LinearLayout buttons = new LinearLayout(activity);
        buttons.setGravity(Gravity.CENTER);
        buttons.setOrientation(LinearLayout.HORIZONTAL);
        buttons.setBackgroundColor(COLOR_HEADER);

        TextView cancel = createActionButton("Cancel", COLOR_MUTED_TEXT);
        cancel.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                cancel();
            }
        });

        TextView done = createActionButton("OK", COLOR_ACTION);
        done.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                commit();
            }
        });

        LinearLayout.LayoutParams buttonParams =
                new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1.0f);
        buttons.addView(cancel, buttonParams);
        buttons.addView(done, new LinearLayout.LayoutParams(
                0, ViewGroup.LayoutParams.MATCH_PARENT, 1.0f));
        return buttons;
    }

    private static TextView createActionButton(String text, int color) {
        TextView button = new TextView(activity);
        button.setText(text);
        button.setTextColor(color);
        button.setTextSize(15.0f);
        button.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        button.setGravity(Gravity.CENTER);
        button.setAllCaps(false);
        button.setBackground(makePressedDrawable());
        button.setClickable(true);
        button.setFocusable(true);
        return button;
    }

    private static WindowManager.LayoutParams createLayoutParams(int editorMode) {
        final boolean multiline = editorMode == MODE_MULTI_LINE;
        WindowManager.LayoutParams params = new WindowManager.LayoutParams(
                multiline ? WindowManager.LayoutParams.MATCH_PARENT : getFloatingPanelWidth(),
                multiline ? getHalfScreenHeight() : WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.TYPE_APPLICATION,
                WindowManager.LayoutParams.FLAG_FULLSCREEN
                        | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN
                        | WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS
                        | WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION,
                PixelFormat.RGBA_8888);

        params.gravity = Gravity.TOP | Gravity.CENTER_HORIZONTAL;
        params.y = multiline ? 0 : dp(24);
        params.softInputMode = WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE
                | WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE;
        applyActivityToken(params);
        return params;
    }

    private static int createInputType(boolean multiline) {
        int inputType = InputType.TYPE_CLASS_TEXT
                | InputType.TYPE_TEXT_FLAG_CAP_SENTENCES
                | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
        if (multiline) {
            inputType |= InputType.TYPE_TEXT_FLAG_MULTI_LINE;
        }
        return inputType;
    }

    private static void commit() {
        if (editor == null) {
            return;
        }

        String text = editor.getText().toString();
        if (mode == MODE_SINGLE_LINE) {
            text = text.replace('\n', ' ');
        }

        GLES3JNIView.commitKeyboardText(text);
        if (MenuOverlay.imGuiSurface != null) {
            MenuOverlay.imGuiSurface.requestRender();
        }
        dismissLocal(false);
    }

    private static void cancel() {
        GLES3JNIView.cancelKeyboardText();
        if (MenuOverlay.imGuiSurface != null) {
            MenuOverlay.imGuiSurface.requestRender();
        }
        dismissLocal(false);
    }

    private static void dismissLocal(boolean cancelNative) {
        if (cancelNative) {
            GLES3JNIView.cancelKeyboardText();
        }

        if (editor != null && activity != null) {
            InputMethodManager inputManager = (InputMethodManager)
                    activity.getSystemService(Context.INPUT_METHOD_SERVICE);
            if (inputManager != null) {
                inputManager.hideSoftInputFromWindow(editor.getWindowToken(), 0);
            }
        }

        if (panel != null && windowManager != null) {
            try {
                windowManager.removeViewImmediate(panel);
            } catch (RuntimeException ignored) {
            }
        }

        panel = null;
        editor = null;
        activeId = 0;
        mode = MODE_NONE;
    }

    private static boolean isEnterUp(KeyEvent event) {
        return event != null
                && event.getKeyCode() == KeyEvent.KEYCODE_ENTER
                && event.getAction() == KeyEvent.ACTION_UP;
    }

    private static int parseInt(String[] values, int index, int fallback) {
        if (values == null || index >= values.length || values[index] == null) {
            return fallback;
        }

        try {
            return Integer.parseInt(values[index]);
        } catch (NumberFormatException ignored) {
            return fallback;
        }
    }

    private static int getHalfScreenHeight() {
        if (activity == null) {
            return WindowManager.LayoutParams.WRAP_CONTENT;
        }
        return Math.max(dp(220), activity.getResources().getDisplayMetrics().heightPixels / 2);
    }

    private static int getFloatingPanelWidth() {
        if (activity == null) {
            return WindowManager.LayoutParams.MATCH_PARENT;
        }
        int screenWidth = activity.getResources().getDisplayMetrics().widthPixels;
        return Math.max(dp(280), screenWidth - dp(24));
    }

    private static GradientDrawable makeRoundedDrawable(
            int color, int radius, int strokeColor, int strokeDp) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(color);
        drawable.setCornerRadius(radius);
        if (strokeDp > 0) {
            drawable.setStroke(dp(strokeDp), strokeColor);
        }
        return drawable;
    }

    private static GradientDrawable makePressedDrawable() {
        return makeRoundedDrawable(0x00000000, dp(8), 0x00000000, 0);
    }

    private static int dp(float value) {
        if (activity == null) {
            return (int) value;
        }
        return (int) (value * activity.getResources().getDisplayMetrics().density + 0.5f);
    }

    private static void applyActivityToken(WindowManager.LayoutParams params) {
        if (params == null || activity == null || activity.getWindow() == null) {
            return;
        }

        params.token = activity.getWindow().getAttributes().token;
        if (params.token == null && activity.getWindow().getDecorView() != null) {
            params.token = activity.getWindow().getDecorView().getApplicationWindowToken();
        }
    }
}
