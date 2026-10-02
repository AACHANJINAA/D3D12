#pragma once
#include "ViewerPanels.h"

class MODEL_IMPORT_DIALOG
{
public:
    void open(VIEWER_REQUEST request);
    void draw();
    bool is_open() const { return _ispending; }
    VIEWER_REQUEST take_request();

private:
    VIEWER_REQUEST _pending;
    VIEWER_REQUEST _ready;
    bool _ispending = false;
    bool _isopening = false;
};
