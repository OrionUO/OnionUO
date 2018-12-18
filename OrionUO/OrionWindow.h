// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Platform.h"

enum
{
    FASTLOGIN_TIMER_ID = 1,
};

class COrionWindow : public Wisp::CWindow
{
public:
    COrionWindow();
    virtual ~COrionWindow();

    void SetRenderTimerDelay(int delay);
    void EmulateOnLeftMouseButtonDown();
    int GetRenderDelay();

    static const uint32_t MessageID = USER_MESSAGE_ID + 402;

protected:
    virtual bool OnCreate();
    virtual void OnDestroy();
    virtual void OnResize();
    virtual void OnLeftMouseButtonDown();
    virtual void OnLeftMouseButtonUp();
    virtual bool OnLeftMouseButtonDoubleClick();
    virtual void OnRightMouseButtonDown();
    virtual void OnRightMouseButtonUp();
    virtual bool OnRightMouseButtonDoubleClick();
    virtual void OnMidMouseButtonDown();
    virtual void OnMidMouseButtonUp();
    virtual bool OnMidMouseButtonDoubleClick();
    virtual void OnMidMouseButtonScroll(bool up);
    virtual void OnXMouseButton(bool up);
    virtual void OnDragging();
    virtual void OnActivate();
    virtual void OnDeactivate();
    virtual void OnShow(bool show);
    virtual void OnSetText(const char *str);
    virtual void OnTimer(uint32_t id);
    virtual bool OnRepaint(const PaintEvent &ev);
    virtual bool OnUserMessages(const UserEvent &ev) override;
    virtual void OnTextInput(const TextEvent &ev) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;
    virtual void OnKeyUp(const KeyEvent &ev) override;

private:
    int m_iRenderDelay;
};

extern COrionWindow g_OrionWindow;
