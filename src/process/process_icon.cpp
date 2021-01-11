/*
* Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd
*
* Author:      maojj <maojunjie@uniontech.com>
* Maintainer:  maojj <maojunjie@uniontech.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "process_icon.h"

#include "process_icon_cache.h"
#include "process.h"
#include "desktop_entry_cache.h"
#include "common/common.h"
#include "system/system_monitor.h"
#include "process/process_db.h"
#include "wm/wm_window_list.h"

#include <QFileInfo>
#include <QImage>

using namespace common::init;
using namespace common::core;
using namespace core::system;
using namespace core::process;
using namespace core::wm;

namespace core {
namespace process {

enum icon_data_type_t {
    kIconDataNameType,
    kIconDataPixmapType
};

struct icon_data_t {
    icon_data_type_t type;
    char __pad__[4];
    QString proc_name;
    bool desktopentry = false;
};

struct icon_data_name_type : public icon_data_t {
    QString icon_name;
};
struct icon_data_pix_map_type : public icon_data_t {
    QMap<uint64_t, QVector<uint>> pixMap;
};

ProcessIcon::ProcessIcon()
{
}

ProcessIcon::~ProcessIcon()
{

}

void ProcessIcon::refreashProcessIcon(Process *proc)
{
    if (proc) {
        ProcessIconCache *cache = ProcessIconCache::instance();
        if (cache->contains(proc->pid())) {
            auto *procIcon = cache->getProcessIcon(proc->pid());
            if (procIcon) {
                m_data = procIcon->m_data;
                if (m_data->desktopentry)
                    ProcessDB::instance()->windowList()->addDesktopEntryApp(proc);
            }
        } else {
            auto iconDataPtr = getIcon(proc);
            m_data = iconDataPtr;
            auto *procIcon = new ProcessIcon();
            procIcon->m_data = iconDataPtr;
            cache->addProcessIcon(proc->pid(), procIcon);
        }
    }
}

QIcon ProcessIcon::icon() const
{
    QIcon icon;

    if (m_data) {
        if (m_data->type == kIconDataNameType) {
            auto *iconData = reinterpret_cast<struct icon_data_name_type *>(m_data.get());
            if (iconData)
                icon = QIcon::fromTheme(iconData->icon_name);
        } else if (m_data->type == kIconDataPixmapType) {
            auto *iconData = reinterpret_cast<struct icon_data_pix_map_type *>(m_data.get());
            if (iconData) {
                QMap<uint64_t, QVector<uint>>::const_iterator it = iconData->pixMap.constBegin();
                while (it != iconData->pixMap.constEnd()) {
                    union size_u sz;
                    sz.k = it.key();
                    QImage img(reinterpret_cast<const uchar *>(it.value().constData()), int(sz.s.w), int(sz.s.h), QImage::Format_ARGB32);
                    auto pix = QPixmap::fromImage(img);
                    icon.addPixmap(pix);
                    ++it;
                } // ::while
            } // ::if(iconData)
        }
    } // ::if(m_data)

    return icon;
}

struct icon_data_t *ProcessIcon::defaultIconData() const {
    auto *iconData = new struct icon_data_name_type();
    iconData->type = kIconDataNameType;
    iconData->proc_name = "[::default::]";
    iconData->icon_name = "application-x-executable";
    return iconData;
}

struct icon_data_t *ProcessIcon::terminalIconData() const {
    auto *iconData = new struct icon_data_name_type();
    iconData->type = kIconDataNameType;
    iconData->proc_name = "[::terminal::]";
    iconData->icon_name = "terminal";
    return iconData;
}

std::shared_ptr<icon_data_t> ProcessIcon::getIcon(Process *proc)
{
    std::shared_ptr<icon_data_t> iconDataPtr;

    auto processDB = ProcessDB::instance();
    WMWindowList *windowList = processDB->windowList();
    DesktopEntryCache *desktopEntryCache = processDB->desktopEntryCache();

    if (windowList->isTrayApp(proc->pid())) {
        if (proc->environ().contains("GIO_LAUNCHED_DESKTOP_FILE")) {
            auto desktopFile = proc->environ()["GIO_LAUNCHED_DESKTOP_FILE"];
            auto entry = desktopEntryCache->entryWithDesktopFile(desktopFile);
            if (entry && !entry->icon.isEmpty()) {
                auto *iconData = new struct icon_data_name_type();
                iconData->type = kIconDataNameType;
                iconData->proc_name = proc->name();
                iconData->icon_name = entry->icon;
                iconDataPtr.reset(iconData);
                return iconDataPtr;
            }
        }
    }

    if (windowList->isGuiApp(proc->pid())) {
        auto pixMap = windowList->getWindowIcon(proc->pid());
        if (pixMap.size() > 0) {
            auto *iconData = new struct icon_data_pix_map_type();
            iconData->pixMap = pixMap;
            iconData->proc_name = proc->name();
            iconData->type = kIconDataPixmapType;
            iconDataPtr.reset(iconData);
            return iconDataPtr;
        }
    }

    if (desktopEntryCache->contains(proc->name())) {
        DesktopEntry entry;
        entry = desktopEntryCache->entry(proc->name());
        if (entry && !entry->icon.isEmpty()) {
            auto *iconData = new struct icon_data_name_type();
            iconData->desktopentry = true;
            iconData->type = kIconDataNameType;
            iconData->proc_name = proc->name();
            iconData->icon_name = entry->icon;
            iconDataPtr.reset(iconData);
            windowList->addDesktopEntryApp(proc);
            return iconDataPtr;
        }
    }

    if (proc->environ().contains("GIO_LAUNCHED_DESKTOP_FILE") && proc->environ().contains("GIO_LAUNCHED_DESKTOP_FILE_PID") && proc->environ()["GIO_LAUNCHED_DESKTOP_FILE_PID"].toInt() == proc->pid()) {
        auto desktopFile = proc->environ()["GIO_LAUNCHED_DESKTOP_FILE"];
        auto entry = desktopEntryCache->entryWithDesktopFile(desktopFile);
        if (entry && !entry->icon.isEmpty()) {
            auto *iconData = new struct icon_data_name_type();
            iconData->desktopentry = true;
            iconData->type = kIconDataNameType;
            iconData->proc_name = proc->name();
            iconData->icon_name = entry->icon;
            iconDataPtr.reset(iconData);
            windowList->addDesktopEntryApp(proc);
            return iconDataPtr;
        }
    }

    if (shellList.contains(proc->name())) {
        iconDataPtr.reset(terminalIconData());
        return iconDataPtr;
    }

    if (!proc->cmdline().isEmpty() && proc->cmdline()[0].startsWith("/opt")) {
        QString fname = QFileInfo(QString(proc->cmdline()[0]).split(' ')[0]).fileName();
        auto entry = desktopEntryCache->entryWithSubName(fname);
        if (entry && !entry->icon.isEmpty()) {
            auto *iconData = new struct icon_data_name_type();
            iconData->type = kIconDataNameType;
            iconData->proc_name = proc->name();
            iconData->icon_name = entry->icon;
            iconDataPtr.reset(iconData);
            return iconDataPtr;
        }
    }

    // fallback to use default icon
    iconDataPtr.reset(defaultIconData());
    return iconDataPtr;
}

} // namespace process
} // namespace core
