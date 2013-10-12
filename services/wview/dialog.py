#      dialog.py
#      
#      Copyright (C) 2013 Yi-Wei Ci <ciyiwei@hotmail.com>
#      
#      This program is free software; you can redistribute it and/or modify
#      it under the terms of the GNU General Public License as published by
#      the Free Software Foundation; either version 2 of the License, or
#      (at your option) any later version.
#      
#      This program is distributed in the hope that it will be useful,
#      but WITHOUT ANY WARRANTY; without even the implied warranty of
#      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#      GNU General Public License for more details.
#      
#      You should have received a copy of the GNU General Public License
#      along with this program; if not, write to the Free Software
#      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#      MA 02110-1301, USA.

import gtk
import threading

WDLG_WIDTH = 328
WDLG_HEIGHT = 60
WDLG_BUTTON_LEFT = 230
WDLG_BUTTON_TOP = 15
WDLG_BUTTON_WIDTH = 90
WDLG_BUTTON_HEIGHT = 30
WDLG_LINE_LEFT = 8
WDLG_LINE_TOP = 15
WDLG_LINE_WIDTH = 215
WDLG_LINE_HEIGHT = 30
   
class WDlg(gtk.Window):
    def _init_ui(self, title, line, length, readonly, combo, button):
        self.set_size_request(WDLG_WIDTH, WDLG_HEIGHT)
        self.set_position(gtk.WIN_POS_CENTER)
        self.connect("delete-event", self.on_delete_event)
        self.set_title(title)
        self.set_resizable(False)
        
        self._button = gtk.Button(button)
        self._button.set_size_request(WDLG_BUTTON_WIDTH, WDLG_BUTTON_HEIGHT)
        self._button.connect("clicked", self.on_clicked)
        
        if not combo:
            self._line = gtk.Entry(max=length)
            self._line.set_text(line)
        else:
            self._line = gtk.combo_box_entry_new_text()
            self._line.append_text(line)
            self._line.set_active(0)
            self._line.connect('changed', self.on_changed)
        self._line.set_size_request(WDLG_LINE_WIDTH, WDLG_LINE_HEIGHT)
        if readonly:
            self._line.set_property("editable", False)
            self._line.set_sensitive(False)
        
        fixed = gtk.Fixed()
        fixed.put(self._button, WDLG_BUTTON_LEFT, WDLG_BUTTON_TOP)
        fixed.put(self._line, WDLG_LINE_LEFT, WDLG_LINE_TOP)
        self.add(fixed)
        self.hide()
        self._closed = True
        self._wait = False
        self._readonly = readonly
        self._combo = combo
        
    def __init__(self, title='', line='', length=1024, readonly=False, combo=False, button=''):
        gtk.Window.__init__(self)
        self._init_ui(title, line, length, readonly, combo, button)
        self._lock = threading.Lock()
        self._lock.acquire()
    
    @property
    def name(self):
        return self.__class__.__name__.lower()
    
    def wait(self):
        if not self._wait:
            self._wait = True
            self._button.set_sensitive(True)
            if not self._readonly:
                self._line.set_sensitive(True)
            self._lock.acquire()
    
    def signal(self):
        if self._wait:
            self._lock.release()
            self._button.set_sensitive(False)
            if not self._readonly:
                self._line.set_sensitive(False)
            self._wait = False
            
    
    def show(self):
        if self._closed:
            self.show_all()
            self._closed = False
        self.wait()
        
    def close(self):
        if not self._closed:
            self.hide()
            self._closed = True
        self.signal()
        
    def is_closed(self):
        return self._closed
        
    def get_line(self):
        if not self._combo:
            return str(self._line.get_text())
        else:
            return str(self._line.get_active_text())
        
    def set_button(self, text):
        self._button.set_label(text)
        
    def set_line(self, text):
        if not self._combo:
            self._line.set_text(text)
        else:
            self._line.append_text(text)
            
    def remove_line(self, pos):
        if self._combo:
            self._line.remove_text(pos)
        
    def on_delete_event(self, widget, event):
        return False
        
    def on_clicked(self, widget):
        pass
    
    def on_changed(self, widget):
        pass
