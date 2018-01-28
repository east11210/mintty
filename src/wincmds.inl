#include <winnls.h>

HWND cmds_wnd = 0;
HWND cmds_list_wnd = 0;
wchar* cmds_no_text = 0;
wchar* cmds_filter = 0;
int cmds_filter_len = 0;
short* cmds_filter_result = 0;
#define CMDS_MAX_FILTER_LEN 128

extern ushort cmd_max_num;
extern ushort cmd_num;
extern wstring *cmd_content;
extern wstring *cmd_title;

#define ListView_InsertColumnW(hwnd,iCol,pcol) (int)SNDMSG((hwnd),LVM_INSERTCOLUMNW,(WPARAM)(int)(iCol),(LPARAM)(const LV_COLUMNW *)(pcol))
#define ListView_GetItemTextW(hwndLV,i,iSubItem_,pszText_,cchTextMax_) { LV_ITEMW _ms_lvi; _ms_lvi.iSubItem = iSubItem_; _ms_lvi.cchTextMax = cchTextMax_; _ms_lvi.pszText = pszText_; SNDMSG((hwndLV),LVM_GETITEMTEXTW,(WPARAM)(i),(LPARAM)(LV_ITEMW *)&_ms_lvi);}

#define ListView_InsertItemW(hwnd, pitem) (int)SNDMSG((hwnd), LVM_INSERTITEMW, 0, (LPARAM)(const LV_ITEMW *)(pitem))
#define ListView_SetItemTextW(hwnd, pitem) SNDMSG((hwnd), LVM_SETITEMTEXTW, (WPARAM)((pitem)->iItem), (LPARAM)(LV_ITEMW *)(pitem))

static wchar*
strtolower(wstring str, wchar* out)
{
  wchar *l = out;
  while (*str) *l++ = tolower(*str++);
  *l = 0;
  return out;
}

static int
command_get_filter_result(int num)
{
  if (0 > num) {
    return -1;
  }
  if (0 >= cmds_filter_len) {
    return num;
  }
  for (int i = 0; i < cmd_num; ++i) {
    if (cmds_filter_result[i] >= cmds_filter_len) {
      if (0 == num--) {
        return i;
      }
    }
  }
  return -1;
}

static void
command_set_list_item(int num)
{
  ListView_SetItemCountEx(cmds_list_wnd, num, 0);
  if (0 < num) {
    ListView_SetItemState(cmds_list_wnd, 0, LVIS_SELECTED | LVIS_FOCUSED,
      LVIS_SELECTED | LVIS_FOCUSED);
    SetFocus(cmds_list_wnd);
  }
}

static void
command_filter(wstring filter)
{
  wchar lower[1024];

  int n = 0;
  for (uint i = 0; i < cmd_num; ++i) {
    if (cmds_filter_result[i] >= cmds_filter_len
      || 0 != wcsstr(strtolower(cmd_title[i], lower), filter)
      || 0 != wcsstr(strtolower(cmd_content[i], lower), filter)) {
      cmds_filter_result[i] = cmds_filter_len;
      ++n;
    } else {
      cmds_filter_result[i] = cmds_filter_len - 1;
    }
  }
  command_set_list_item(n);
}

static void
command_filter_reset()
{
  cmds_filter_len = 0;
  memset(cmds_filter_result, 0, cmd_num * sizeof(cmds_filter_result[0]));
  command_set_list_item(cmd_num);
}

static void
command_filter_pop()
{
  if (0 >= cmds_filter_len) {
    return;
  }
  cmds_filter[--cmds_filter_len] = 0;
  SetWindowTextW(cmds_wnd, cmds_filter);
  if (0 < cmds_filter_len) {
    command_filter(cmds_filter);
  } else {
    command_filter_reset();
  }
}

static void
command_filter_push(int c)
{
  if (CMDS_MAX_FILTER_LEN <= (cmds_filter_len + 1)) {
    return;
  }
  cmds_filter[cmds_filter_len++] = (wchar) c;
  cmds_filter[cmds_filter_len] = 0;
  SetWindowTextW(cmds_wnd, cmds_filter);
  command_filter(cmds_filter);
}

static int
command_get_content(wstring raw, wchar* out)
{
  wchar *p = out;
  for (; *raw; ++p, ++raw) {
    if ('\\' == *raw) {
      switch (raw[1]) {
        when 'n' : *p = '\n';
        when 'r' : *p = '\r';
        when '\\' : *p = '\\';
        when 'N' : { *p++ = '\\'; *p = 'n'; }
        otherwise: { *p = '\\'; continue; }
      }
      ++raw;
    } else {
      *p = *raw;
    }
  }
  return p - out;
}

static INT_PTR CALLBACK
command_dialog_proc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  (void)wParam;
  switch (msg) {
    when WM_INITDIALOG: {
      SendMessage(wnd, WM_SETICON, (WPARAM) ICON_BIG,
                  (LPARAM) LoadIcon(inst, MAKEINTRESOURCE(IDI_MAINICON)));
      cmds_no_text = malloc(32 * sizeof(cmds_no_text[0]));
      cmds_filter = malloc(CMDS_MAX_FILTER_LEN * sizeof(cmds_filter[0]));
      cmds_filter_result = malloc(cmd_num * sizeof(cmds_filter_result[0]));
      memset(cmds_filter_result, 0, cmd_num * sizeof(cmds_filter_result[0]));

      #define CLIENT_EDGE 3
      RECT r;
      GetClientRect(wnd, &r);
      InflateRect(&r, -CLIENT_EDGE, -CLIENT_EDGE);

      cmds_list_wnd =
          CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT |
                              LVS_NOSORTHEADER | LVS_OWNERDATA | LVS_SINGLESEL,
                          r.left, r.top, r.right - r.left,
                          r.bottom - r.top, wnd, null, inst,
                          null);
      ListView_SetExtendedListViewStyle(cmds_list_wnd,
                LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

      LVCOLUMN lc = {
        .mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT,
        .fmt = LVCFMT_LEFT,
      };
      lc.cx = 40;
      lc.pszText = "No.";
      ListView_InsertColumn(cmds_list_wnd, 0, &lc);
      lc.cx = 200;
      lc.pszText = "Title";
      ListView_InsertColumn(cmds_list_wnd, 1, &lc); 
      lc.cx = 500;
      lc.pszText = "Command";
      ListView_InsertColumn(cmds_list_wnd, 2, &lc); 
      // cmds_ctrlbox = ctrl_new_box();
      // setup_cmds_box(cmds_ctrlbox);

      command_set_list_item(cmd_num);
    }

    // when WM_NOTIFYFORMAT :
    //   fprintf(stderr, "notifyformat %p, %p, %p\n", wnd, cmds_list_wnd, (HWND)wParam);
    //   return NFR_UNICODE;

    when WM_NOTIFY:
    {
      switch (((LPNMHDR) lParam)->code) {
        when LVN_GETDISPINFOW: {
          NMLVDISPINFOW *plvdi = (NMLVDISPINFOW *)lParam;
          switch (plvdi->item.iSubItem) {
          when 0: {
              // rgPetInfo is an array of PETINFO structures.
              wsprintfW(cmds_no_text, L"%d", plvdi->item.iItem);
              plvdi->item.pszText = cmds_no_text;
            }

          when 1: {
              int item = command_get_filter_result(plvdi->item.iItem);
              if (0 <= item) {
                plvdi->item.pszText = (wchar*) cmd_title[item];
              } else {
                plvdi->item.pszText = L"";
              }
            }

          when 2: {
              int item = command_get_filter_result(plvdi->item.iItem);
              if (0 <= item) {
                plvdi->item.pszText = (wchar*) cmd_content[item];
              } else {
                plvdi->item.pszText = L"";
              }
            }
          }
          return TRUE;
        }

        when LVN_KEYDOWN: {
          ushort key = ((LPNMLVKEYDOWN)lParam)->wVKey;
          switch (key) {
            when VK_RETURN: {
              int item = ListView_GetNextItem(cmds_list_wnd, -1, LVNI_ALL | LVNI_SELECTED);
              wchar content[1024], command[1024];
              ListView_GetItemTextW(cmds_list_wnd, item, 2, content, lengthof(content));
              term_paste(command, command_get_content(content, command));
              DestroyWindow(cmds_wnd);
            }
            when VK_ESCAPE:
              if (0 < cmds_filter_len) {
                command_filter_reset();
              } else {
                DestroyWindow(cmds_wnd);
              }

            when VK_BACK or VK_DELETE:
              command_filter_pop();

            when 'a' ... 'z' or 'A' ... 'Z' or '0' ... '9':
              command_filter_push(tolower(key));
          }
        }
      }
    }

    when WM_CLOSE:
      DestroyWindow(wnd);

    when WM_DESTROY:
      cmds_wnd = 0;
      cmds_list_wnd = 0;
      free(cmds_no_text);
      cmds_no_text = 0;
      free(cmds_filter);
      cmds_filter = 0;
      free(cmds_filter_result);
      cmds_filter_result = 0;
      cmds_filter_len = 0;
#ifdef debug_dialog_crash
      signal(SIGSEGV, SIG_DFL);
#endif
  }
  return 0;
}

void
win_open_commands(void)
{
  if (cmds_wnd)
    return;

#ifdef debug_dialog_crash
  signal(SIGSEGV, sigsegv);
#endif

  set_dpi_auto_scaling(true);

  static bool initialised = false;
  if (!initialised)
  {
    InitCommonControls();
    RegisterClassW(&(WNDCLASSW){
        .lpszClassName = W(CMD_DIALOG_CLASS),
        .lpfnWndProc = DefDlgProcW,
        .style = CS_DBLCLKS,
        .cbClsExtra = 0,
        .cbWndExtra = DLGWINDOWEXTRA + 2 * sizeof(LONG_PTR),
        .hInstance = inst,
        .hIcon = null,
        .hCursor = LoadCursor(null, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1),
        .lpszMenuName = null});
    initialised = true;
  }

  hook_windows(scale_options);
  cmds_wnd = CreateDialogW(inst, MAKEINTRESOURCEW(IDD_CMDSBOX),
                          wnd, command_dialog_proc);
  unhook_windows();
  // At this point, we could actually calculate the size of the
  // dialog box used for the Options menu; however, the resulting
  // value(s) (here DIALOG_HEIGHT) is already needed before this point,
  // as the callback config_dialog_proc sets up dialog box controls.
  // How insane is that resource concept! Shouldn't I know my own geometry?
  determine_geometry(cmds_wnd); // dummy call

  // Set title of Options dialog explicitly to facilitate I18N
  //__ Options: dialog title
  SendMessageW(cmds_wnd, WM_SETTEXT, 0, (LPARAM)_W("Press key to filter"));
  RECT r;
  GetWindowRect(wnd, &r);
  const int x = (r.right + r.left) / 2;
  const int y = (r.bottom + r.top) / 2;
  GetClientRect(cmds_wnd, &r);
  SetWindowPos(cmds_wnd, HWND_TOP, x - r.right / 2, y - r.bottom / 2, 0, 0,
               SWP_NOSIZE | SWP_SHOWWINDOW);
  // ShowWindow(cmds_wnd, SW_SHOW);

  set_dpi_auto_scaling(false);
}