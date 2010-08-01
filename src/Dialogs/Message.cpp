/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009

	M Roberts (original release)
	Robin Birch <robinb@ruffnready.co.uk>
	Samuel Gisiger <samuel.gisiger@triadis.ch>
	Jeff Goodenough <jeff@enborne.f2s.com>
	Alastair Harrison <aharrison@magic.force9.co.uk>
	Scott Penrose <scottp@dd.com.au>
	John Wharington <jwharington@gmail.com>
	Lars H <lars_hn@hotmail.com>
	Rob Dunning <rob@raspberryridgesheepfarm.com>
	Russell King <rmk@arm.linux.org.uk>
	Paolo Ventafridda <coolwind@email.it>
	Tobias Lohner <tobias@lohner-net.de>
	Mirek Jezek <mjezek@ipplc.cz>
	Max Kellermann <max@duempel.org>
	Tobias Bieniek <tobias.bieniek@gmx.de>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "Dialogs/Message.hpp"
#include "Language.hpp"
#include "Form/Button.hpp"
#include "Form/Form.hpp"
#include "Form/Frame.hpp"
#include "Form/Edit.hpp"
#include "MainWindow.hpp"
#include "Screen/Fonts.hpp"
#include "Screen/Layout.hpp"
#include "Interface.hpp"

#include <assert.h>
#include <limits.h>

class ModalResultButton : public WndButton {
  WndForm &form;
  int result;

public:
  ModalResultButton(ContainerControl &Parent, const TCHAR *Caption,
                    int X, int Y, int Width, int Height,
                    const WindowStyle style,
                    WndForm &_form, int _result)
    :WndButton(Parent.GetClientAreaWindow(), Caption, X, Y, Width, Height,
               style, Parent.GetBackColor()),
     form(_form), result(_result) {}

protected:
  virtual void on_click() {
    form.SetModalResult(result);
  }
};

// Message Box Replacement
/**
 * Displays a MessageBox and returns the pressed button
 * @param lpText Text displayed inside the MessageBox
 * @param lpCaption Text displayed in the Caption of the MessageBox
 * @param uType Type of MessageBox to display (OK+Cancel, Yes+No, etc.)
 * @return
 */
int WINAPI
MessageBoxX(LPCTSTR lpText, LPCTSTR lpCaption, unsigned uType)
{
  WndFrame *wText = NULL;
  int X, Y, Width, Height;
  WndButton *wButtons[10];
  int ButtonCount = 0;
  int i, x, y, d, w, h, res, dY;
  RECT rc;

  assert(lpText != NULL);

  // JMW this makes the first key if pressed quickly, ignored
  // TODO bug: doesn't work sometimes. buttons have to be pressed multiple times (TB)
  XCSoarInterface::Debounce();

  rc = XCSoarInterface::main_window.get_screen_position();

#ifdef ALTAIRSYNC
  Width = Layout::Scale(220);
  Height = Layout::Scale(160);
#else
  Width = Layout::Scale(200);
  Height = Layout::Scale(160);
#endif

  X = ((rc.right - rc.left) - Width) / 2;
  Y = ((rc.bottom - rc.top) - Height) / 2;

  y = Layout::Scale(100);
  w = Layout::Scale(60);
  h = Layout::Scale(32);

  // Create dialog
  WindowStyle style;
  style.hide();

  WndForm wf(XCSoarInterface::main_window,
             lpCaption, X, Y, Width, Height,
             style);
  wf.SetFont(Fonts::MapBold);
  wf.SetTitleFont(Fonts::MapBold);
  wf.SetBackColor(Color(0xDA, 0xDB, 0xAB));

  // Create text element
  wText = new WndFrame(wf, 0, Layout::Scale(5), Width, Height);

  wText->SetCaption(lpText);
  wText->SetFont(Fonts::MapBold);
  wText->SetAlignCenter();
  // | DT_VCENTER

  /* TODO code: this doesnt work to set font height
  dY = wText->GetLastDrawTextHeight() - Height;
  */
  dY = Layout::Scale(-40);
  wText->resize(Width, wText->GetTextHeight() + 5);
  wf.resize(Width, wf.get_size().cy + dY);

  y += dY;

  // Create buttons
  WindowStyle button_style;
  button_style.tab_stop();

  uType = uType & 0x000f;
  if (uType == MB_OK || uType == MB_OKCANCEL) {
    wButtons[ButtonCount] =
      new ModalResultButton(wf, _("OK"), 0, y, w, h,
                            button_style, wf, IDOK);

    ButtonCount++;
  }

  if (uType == MB_YESNO || uType == MB_YESNOCANCEL) {
    wButtons[ButtonCount] =
      new ModalResultButton(wf, _("Yes"), 0, y, w, h,
                            button_style, wf, IDYES);

    ButtonCount++;

    wButtons[ButtonCount] =
      new ModalResultButton(wf, _("No"), 0, y, w, h,
                            button_style, wf, IDNO);

    ButtonCount++;
  }

  if (uType == MB_ABORTRETRYIGNORE || uType == MB_RETRYCANCEL) {
    wButtons[ButtonCount] =
      new ModalResultButton(wf, _("Retry"), 0, y, w, h,
                            button_style, wf, IDRETRY);

    ButtonCount++;
  }

  if (uType == MB_OKCANCEL || uType == MB_RETRYCANCEL || uType == MB_YESNOCANCEL) {
    wButtons[ButtonCount] =
      new ModalResultButton(wf, _("Cancel"), 0, y, w, h,
                            button_style, wf, IDCANCEL);

    ButtonCount++;
  }

  if (uType == MB_ABORTRETRYIGNORE) {
    wButtons[ButtonCount] =
      new ModalResultButton(wf, _("Abort"), 0, y, w, h,
                            button_style, wf, IDABORT);

    ButtonCount++;

    wButtons[ButtonCount] =
      new ModalResultButton(wf, _("Ignore"), 0, y, w, h,
                            button_style, wf, IDIGNORE);

    ButtonCount++;
  }

  d = Width / (ButtonCount);
  x = d / 2 - w / 2;

  // Move buttons to the right positions
  for (i = 0; i < ButtonCount; i++) {
    wButtons[i]->move(x, y);
    x += d;
  }

  // Show MessageBox and save result
  res = wf.ShowModal();

  delete wText;
  for (int i = 0; i < ButtonCount; ++i)
    delete wButtons[i];
  wf.reset();

#ifdef ALTAIRSYNC
  // force a refresh of the window behind
  InvalidateRect(hWnd,NULL,true);
  UpdateWindow(hWnd);
#endif

  return(res);
}
