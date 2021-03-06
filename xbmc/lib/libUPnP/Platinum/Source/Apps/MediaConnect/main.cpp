/*****************************************************************
|
|      Platinum - Test UPnP A/V Media Connect
|
| Copyright (c) 2004-2008, Plutinosoft, LLC.
| All rights reserved.
| http://www.plutinosoft.com
|
| This program is free software; you can redistribute it and/or
| modify it under the terms of the GNU General Public License
| as published by the Free Software Foundation; either version 2
| of the License, or (at your option) any later version.
|
| OEMs, ISVs, VARs and other distributors that combine and 
| distribute commercially licensed software with Platinum software
| and do not wish to distribute the source code for the commercially
| licensed software under version 2, or (at your option) any later
| version, of the GNU General Public License (the "GPL") must enter
| into a commercial license agreement with Plutinosoft, LLC.
| 
| This program is distributed in the hope that it will be useful,
| but WITHOUT ANY WARRANTY; without even the implied warranty of
| MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
| GNU General Public License for more details.
|
| You should have received a copy of the GNU General Public License
| along with this program; see the file LICENSE.txt. If not, write to
| the Free Software Foundation, Inc., 
| 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
| http://www.gnu.org/licenses/gpl-2.0.html
|
****************************************************************/

/*----------------------------------------------------------------------
|       includes
+---------------------------------------------------------------------*/
#include "PltUPnP.h"
#include "PltMediaConnect.h"

/*----------------------------------------------------------------------
|       main
+---------------------------------------------------------------------*/
int
main(int argc, char** argv)
{
    NPT_COMPILER_UNUSED(argc);
    NPT_COMPILER_UNUSED(argv);

    PLT_UPnP upnp;
 
    PLT_DeviceHostReference device(new PLT_MediaConnect("C:\\", "Platinum: Sylvain: "));
    upnp.AddDevice(device);
//    NPT_String uuid = device->GetUUID();
//
//    PLT_CtrlPoint* ctrlPoint = new PLT_CtrlPoint(uuid);
//    PLT_MediaBrowser* browser = new PLT_MediaBrowser(ctrlPoint, NULL);
//    upnp.AddCtrlPoint(ctrlPoint);
//    ctrlPoint->Release();

    if (NPT_FAILED(upnp.Start()))
        return 1;

    char buf[256];
    while (gets(buf))
    {
        if (*buf == 'q')
        {
            break;
        }
    }

    upnp.Stop();
//    delete browser;
    return 0;
}
