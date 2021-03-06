/*  Copyright (C) 2014 Xander Vedejas <xvedejas@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Maintained by:
 *      Xander Vedejas <xvedejas@gmail.com>
 */
#ifndef __rtl8139_h__
#define __rtl8139_h__

#include <main.h>
#include <pci.h>
#include <nic.h>

extern void rtl8139_invoke_interrupt(PciConfigHeaderBasic *pci);
extern void rtl8139_probe(PciConfigHeaderBasic *pci);
extern void rtl_install(PciConfigHeaderBasic *pci);


#endif // __rtl8139_h__
