#      connector.py
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

import re
from dialog import WDlg
    
class Connector(WDlg):    
    def __init__(self):
        self._records = ['user@x.x.x.x']
        self._enter = False
        WDlg.__init__(self, title='WING', line=self._records[0], length=32, combo=True, button='connect')
    
    def get_args(self, line=None):
        addr = ''
        name = ''
        if not line:
            line = self.get_line()
        m = re.match('(\w+)@((\d{1,3}\.){3}\d{1,3})', line)
        try:
            addr = str(m.group(2))
            name = str(m.group(1))
        finally:
            return (addr, name)
    
    def has_record(self):
        addr, name = self.get_args()
        record = {'addr':addr, 'name':name}
        if record in self._records:
            return True
        else:
            return False
        
    def remove_record(self):
        addr, name = self.get_args()
        record = {'addr':addr, 'name':name}
        for i in range(len(self._records)):
            if i > 0 and self._records[i] == record:
                del self._records[i]
                self.remove_line(i)
                break
    
    def wait_for_results(self):
        self.show()
        if self.is_closed():
            return
        addr, name = self.get_args()
        if self._enter:
            return {'enter':True, 'addr':addr, 'name':name}
        else:
            self.remove_record()
            return {'enter':False, 'addr':addr, 'name':name}
        
    def enter(self):
        self.set_button('enter')
        self._enter = True
        return self.wait_for_results()
    
    def add_record(self, addr, name):
        record = {'addr':addr, 'name':name}
        if record not in self._records:
            line = self.get_line()
            if (addr, name) == self.get_args(line):
                self._records.append(record)
                self.set_line(line)
            
    def exit(self, addr, name):
        self.add_record(addr, name)
        self.set_button('exit')
        self._enter = False
        return self.wait_for_results()
            
    def on_clicked(self, widget):
        self.signal()
    
    def on_delete_event(self, widget, event):
        self.close()
        return True
    
    def on_changed(self, widget):
        if self.has_record():
            self.set_button('exit')
            self._enter = False
        else:
            self.set_button('enter')
            self._enter = True
            
        
    