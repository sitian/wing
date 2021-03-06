#      acceptor.py
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

from dialog import WDlg

class Acceptor(WDlg):
    def __init__(self):
        WDlg.__init__(self, title='WING', readonly=True, length=32, button='accept')
        self._accept = False
    
    def accept(self, text):
        self._accept = False
        self.set_line(text)
        self.show()
        return self._accept
    
    def on_clicked(self, widget):
        self._accept = True
        self.close()
    
    def on_delete_event(self, widget, event):
        self.close()
        return True
    