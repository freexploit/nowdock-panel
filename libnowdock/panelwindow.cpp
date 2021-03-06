#include "panelwindow.h"

#include "xwindowinterface.h"

#include <QMenu>
#include <QQuickWindow>
#include <QRegion>
#include <QScreen>
#include <QTimer>
#include <QWindow>

#include <QDebug>

#include <KActionCollection>
#include <KAuthorized>
#include <KLocalizedString>
#include <KPluginInfo>

#include <Plasma/Applet>
#include <Plasma/Containment>
#include <Plasma/ContainmentActions>
#include <Plasma/Corona>
#include <PlasmaQuick/AppletQuickItem>

namespace NowDock
{

PanelWindow::PanelWindow(QQuickWindow *parent) :
    QQuickWindow(parent),
    m_immutable(true),
    m_disableHiding(false),
    m_isAutoHidden(false),
    m_isHovered(false),
    m_secondInitPass(false),
    m_windowIsInAttention(false),
    m_childrenLength(-1),
    m_tempThickness(-1)
{    
    setClearBeforeRendering(true);
    setColor(QColor(Qt::transparent));
    setFlags(Qt::Tool|Qt::FramelessWindowHint|Qt::WindowDoesNotAcceptFocus);

    m_interface = new XWindowInterface(this);
    connect(m_interface, SIGNAL(windowInAttention(bool)), this, SLOT(setWindowInAttention(bool)));
    //connect(m_interface, SIGNAL(windowChanged()), this, SLOT(windowChanged()));
    connect(m_interface, SIGNAL(activeWindowChanged()), this, SLOT(activeWindowChanged()));
    m_interface->setDockToAllDesktops();

    m_screen = screen();
    connect(this, SIGNAL(screenChanged(QScreen *)), this, SLOT(screenChanged(QScreen *)));
    if (m_screen) {
        connect(m_screen, SIGNAL(geometryChanged(const QRect &)), this, SIGNAL(screenGeometryChanged()));
    }

    m_updateStateTimer.setSingleShot(true);
    m_updateStateTimer.setInterval(1500);
    connect(&m_updateStateTimer, &QTimer::timeout, this, &PanelWindow::updateState);

    m_initTimer.setSingleShot(true);
    m_initTimer.setInterval(400);
    connect(&m_initTimer, &QTimer::timeout, this, &PanelWindow::initWindow);

    connect(this, SIGNAL(panelVisibilityChanged()), this, SLOT(updateVisibilityFlags()));
    setPanelVisibility(BelowActive);
    updateVisibilityFlags();

    connect(this, SIGNAL(locationChanged()), this, SLOT(updateWindowPosition()));
    connect(this, SIGNAL(windowInAttentionChanged()), this, SLOT(updateState()));

    initialize();
}

PanelWindow::~PanelWindow()
{
    qDebug() << "Destroying Now Dock - Magic Window";
}

QRect PanelWindow::maskArea() const
{
    return m_maskArea;
}

void PanelWindow::setMaskArea(QRect area)
{
    if (m_maskArea == area) {
        return;
    }

    m_maskArea = area;
    m_interface->setMaskArea(area);

    setMask(m_maskArea);

    emit maskAreaChanged();
}

Plasma::Types::Location PanelWindow::location() const
{
    return m_location;
}

void PanelWindow::setLocation(Plasma::Types::Location location)
{
    if (m_location == location) {
        return;
    }

    m_location = location;

    setPanelOrientation(m_location);

    emit locationChanged();
}

unsigned int PanelWindow::maximumLength() const
{
    return m_maximumLength;
}

void PanelWindow::updateMaximumLength()
{
    if (!transientParent()) {
        return;
    }

    unsigned int length = 0;

    if (m_panelOrientation == Qt::Horizontal) {
        length = transientParent()->maximumWidth();
    } else {
        length = transientParent()->maximumHeight();
    }

    if (m_maximumLength == length) {
        return;
    }

    m_maximumLength = length;

    emit maximumLengthChanged();
}

PanelWindow::PanelVisibility PanelWindow::panelVisibility() const
{
    return m_panelVisibility;
}

void PanelWindow::setPanelVisibility(PanelWindow::PanelVisibility state)
{
    if (m_panelVisibility == state) {
        return;
    }

    m_panelVisibility = state;
    emit panelVisibilityChanged();
}

QRect PanelWindow::screenGeometry() const
{
    return (m_screen ? m_screen->geometry() : QRect());
}

bool PanelWindow::isAutoHidden() const
{
    return m_isAutoHidden;
}

void PanelWindow::setIsAutoHidden(bool state)
{
    if (m_isAutoHidden == state) {
        return;
    }

    m_isAutoHidden = state;
    emit isAutoHiddenChanged();
}


bool PanelWindow::windowInAttention() const
{
    return m_windowIsInAttention;
}

void PanelWindow::setWindowInAttention(bool state)
{
    if (m_windowIsInAttention == state) {
        return;
    }

    m_windowIsInAttention = state;
    emit windowInAttentionChanged();
}

int PanelWindow::childrenLength() const
{
    return m_childrenLength;
}

void PanelWindow::setChildrenLength(int value)
{
    if (m_childrenLength == value) {
        return;
    }

    m_childrenLength = value;
    emit childrenLengthChanged();
}

bool PanelWindow::disableHiding() const
{
    return m_disableHiding;
}

void PanelWindow::setDisableHiding(bool value)
{
    if (m_disableHiding == value) {
        return;
    }

    m_disableHiding = value;

    emit disableHidingChanged();

    if (!m_disableHiding) {
        m_updateStateTimer.start();
    }
}

bool PanelWindow::immutable() const
{
    return m_immutable;
}

void PanelWindow::setImmutable(bool state)
{
    if (m_immutable == state) {
        return;
    }

    m_immutable = state;

    emit immutableChanged();
}


bool PanelWindow::isHovered() const
{
    return m_isHovered;
}

void PanelWindow::setIsHovered(bool state)
{
    if (m_isHovered == state) {
        return;
    }

    m_isHovered = state;
    emit isHoveredChanged();
}

void PanelWindow::setPanelOrientation(Plasma::Types::Location location)
{
    if ((location == Plasma::Types::LeftEdge) || (location == Plasma::Types::RightEdge)) {
        m_panelOrientation = Qt::Vertical;
    } else {
        m_panelOrientation = Qt::Horizontal;
    }
}

void PanelWindow::setTransientThickness(unsigned int thickness)
{
    QWindow *transient = transientParent();

    if ((thickness>0) && transient) {
        unsigned int newSize = thickness;

        if (transient) {
            if (m_location == Plasma::Types::BottomEdge) {
                transient->setMinimumHeight(newSize);
                transient->setMaximumHeight(newSize);
                transient->setY(screen()->geometry().y()+screen()->size().height() - newSize);
            } else if (m_location == Plasma::Types::TopEdge) {
                transient->setMinimumHeight(newSize);
                transient->setMaximumHeight(newSize);
                transient->setHeight(newSize);

                transient->setY(screen()->geometry().y());
            } else if (m_location == Plasma::Types::LeftEdge) {
                transient->setMinimumWidth(newSize);
                transient->setMaximumWidth(newSize);
                transient->setWidth(newSize);

                transient->setX(screen()->geometry().x());
            } else if (m_location == Plasma::Types::RightEdge) {
                transient->setMinimumWidth(newSize);
                transient->setMaximumWidth(newSize);
                transient->setWidth(newSize);

                transient->setX(screen()->geometry().x()+screen()->size().width() - newSize);
            }

            if (m_tempThickness < 0) {
                m_tempThickness=newSize;
                m_secondInitPass = false;
                m_initTimer.start();
            } else {
                m_tempThickness = -1;
            }
        }
    }
}

/******************************/
void PanelWindow::addAppletItem(QObject *item)
{
    PlasmaQuick::AppletQuickItem *dynItem = qobject_cast<PlasmaQuick::AppletQuickItem *>(item);

    if (dynItem && !m_containment) {
        Plasma::Applet *applet = dynItem->applet();
        if (applet) {
            m_containment = applet->containment();
        }
    }

    if (!dynItem || m_appletItems.contains(dynItem)) {
        return;
    }

    m_appletItems.append(dynItem);
}

void PanelWindow::removeAppletItem(QObject *item)
{
    PlasmaQuick::AppletQuickItem *dynItem = qobject_cast<PlasmaQuick::AppletQuickItem *>(item);

    if (!dynItem) {
        return;
    }

    m_appletItems.removeAll(dynItem);
}

/*******************************/

void PanelWindow::initialize()
{
    m_secondInitPass = true;
    m_initTimer.start();
}

void PanelWindow::initWindow()
{
    updateVisibilityFlags();

    if (m_tempThickness < 0) {
        shrinkTransient();
    } else {
        setTransientThickness(m_tempThickness);
    }

    updateWindowPosition();

    // The initialization phase makes two passes because
    // changing the window style and type wants a small delay
    // and afterwards the second pass positions them correctly
    if(m_secondInitPass) {
        m_initTimer.start();
        m_secondInitPass = false;
    }
}

void PanelWindow::shrinkTransient()
{
    if (m_immutable && transientParent()) {
        updateMaximumLength();

        int newSize = 15;
        int transWidth = transientParent()->width();
        int transHeight = transientParent()->height();
        int centerX = x()+width()/2;
        int centerY = y()+height()/2;

        QWindow *transient = transientParent();
        int screenLength = ((m_location == Plasma::Types::BottomEdge) || (m_location == Plasma::Types::TopEdge)) ?
                    m_screen->geometry().width() : m_screen->geometry().height();

        int tempLength = qMax(screenLength/2, m_childrenLength);

        if (transient) {
            if (m_location == Plasma::Types::BottomEdge) {
                transient->setMinimumHeight(0);
                transient->setHeight(newSize);
                transient->setMinimumWidth(tempLength);
                transient->setWidth(tempLength);

                transient->setY(screen()->geometry().y()+screen()->size().height() - newSize);
                transient->setX(centerX - transWidth/2);
            } else if (m_location == Plasma::Types::TopEdge) {
                transient->setMinimumHeight(0);
                transient->setHeight(newSize);
                transient->setMinimumWidth(tempLength);
                transient->setWidth(tempLength);

                transient->setY(screen()->geometry().y());
                transient->setX(centerX - transWidth/2);
            } else if (m_location == Plasma::Types::LeftEdge) {
                transient->setMinimumWidth(0);
                transient->setWidth(newSize);
                transient->setMinimumHeight(tempLength);
                transient->setHeight(tempLength);

                transient->setX(screen()->geometry().x());
                transient->setY(centerY - transHeight/2);
            } else if (m_location == Plasma::Types::RightEdge) {
                transient->setMinimumWidth(0);
                transient->setWidth(newSize);
                transient->setMinimumHeight(tempLength);
                transient->setHeight(tempLength);

                transient->setX(screen()->geometry().x()+screen()->size().width() - newSize);
                transient->setY(centerY - transHeight/2);
            }
        }
    }
}

void PanelWindow::updateWindowPosition()
{
    if (m_location == Plasma::Types::BottomEdge) {
        setX(m_screen->geometry().x());
        setY(m_screen->geometry().height() - height());
    } else if (m_location == Plasma::Types::TopEdge) {
        setX(m_screen->geometry().x());
        setY(m_screen->geometry().y());
    } else if (m_location == Plasma::Types::LeftEdge) {
        setX(m_screen->geometry().x());
        setY(m_screen->geometry().y());
    } else if (m_location == Plasma::Types::RightEdge) {
        setX(m_screen->geometry().width() - width());
        setY(m_screen->geometry().y());
    }
}

void PanelWindow::updateVisibilityFlags()
{
    setFlags(Qt::Tool|Qt::FramelessWindowHint|Qt::WindowDoesNotAcceptFocus);
    m_interface->setDockToAllDesktops();

    if (m_panelVisibility == AlwaysVisible) {
        m_interface->setDockToAlwaysVisible();
        updateWindowPosition();
    } else {
        updateWindowPosition();
        showOnTop();
        m_updateStateTimer.start();
    }

    if (m_panelVisibility == AutoHide) {
        m_updateStateTimer.setInterval(2500);
    } else {
        m_updateStateTimer.setInterval(1500);
    }
}

void PanelWindow::menuAboutToHide()
{
    setDisableHiding(false);
    m_updateStateTimer.start();
}

/*
 * It is used from the m_updateStateTimer in order to check the dock's
 * visibility and trigger events and actions which are needed to
 * respond accordingly
 */
void PanelWindow::updateState()
{
    //qDebug() << "in update state disableHiding:" <<m_disableHiding;

    //update the dock behavior
    switch (m_panelVisibility) {
    case BelowActive:
        if ( !m_interface->desktopIsActive() && m_interface->dockIntersectsActiveWindow() ) {
            if ( m_interface->dockIsOnTop() ) {
                if (!m_isHovered && !m_windowIsInAttention && !m_disableHiding) {
                    mustBeLowered();                    //showNormal();
                }
            } else {
                if ( m_windowIsInAttention ) {
                    mustBeRaised();                     //showOnTop();
                }
            }
        } else if (m_interface->dockInNormalState()){
            if(!m_interface->desktopIsActive() && m_interface->dockIsCovered()) {
                mustBeRaised();
            } else {
                showOnTop();
            }
        }
        break;
    case BelowMaximized:
        if ( !m_interface->desktopIsActive() && m_interface->activeIsMaximized() && m_interface->dockIntersectsActiveWindow() ) {
            if ( m_interface->dockIsOnTop() ) {
                if (!m_isHovered && !m_windowIsInAttention && !m_disableHiding) {
                    mustBeLowered();                    //showNormal();
                }
            } else {
                if ( m_windowIsInAttention ) {
                    mustBeRaised();                     //showOnTop();
                }
            }
        } else if ( m_interface->dockInNormalState() ) {
            if(!m_interface->desktopIsActive() && m_interface->dockIsCovered()) {
                mustBeRaised();
            } else {
                showOnTop();
            }
        }
        break;
    case LetWindowsCover:
        if (!m_isHovered && m_interface->dockIsOnTop()) {
            if( m_interface->dockIsCovering()  ) {
                if (!m_disableHiding) {
                    mustBeLowered();
                }
            } else {
                showOnBottom();
            }
        } else if ( m_windowIsInAttention ) {
            if( !m_interface->dockIsOnTop() ) {
                if (m_interface->dockIsCovered()) {
                    mustBeRaised();
                } else {
                    showOnTop();
                }
            }
        }
        break;
    case WindowsGoBelow:
        //Do nothing, the dock is OnTop state in every case
        break;
    case AutoHide:
        if (m_windowIsInAttention && m_isAutoHidden) {
            emit mustBeRaised();
        } else if (!m_isHovered && !m_disableHiding) {
            emit mustBeLowered();
        }
        break;
    case AlwaysVisible:
        //Do nothing, the dock in OnTop state in every case
        break;
    }

}

void PanelWindow::showOnTop()
{
    //    qDebug() << "reached make top...";
    m_interface->showDockOnTop();
}

void PanelWindow::showNormal()
{
    //    qDebug() << "reached make normal...";
    m_interface->showDockAsNormal();
}

void PanelWindow::showOnBottom()
{
    //    qDebug() << "reached make bottom...";
    m_interface->showDockOnBottom();
}


/***************/
void PanelWindow::activeWindowChanged()
{
    if ( (m_panelVisibility == WindowsGoBelow)
         || (m_panelVisibility == AlwaysVisible)
         || (m_panelVisibility == AutoHide)) {
        return;
    }

    //this check is important because otherwise the signals are so often
    //that the timer is never triggered
    if ( !m_updateStateTimer.isActive() ) {
        m_updateStateTimer.start();
    }
}

bool PanelWindow::event(QEvent *event)
{
    if (!event) {
        return false;
    }

    QQuickWindow::event(event);

    if (event->type() == QEvent::Enter) {
        setIsHovered(true);
        m_updateStateTimer.stop();
        shrinkTransient();

        if (m_panelVisibility == AutoHide) {
            if (m_isAutoHidden) {
                emit mustBeRaised();
            }
        } else {
            showOnTop();
        }
    } else if ((event->type() == QEvent::Leave) && (!isActive()) ) {
        setIsHovered(false);

        if ( (m_panelVisibility != WindowsGoBelow)
             && (m_panelVisibility != AlwaysVisible) ) {
            m_updateStateTimer.start();
        }
    }

    return true;
}

void PanelWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (!event) {
        return;
    }

    QQuickWindow::mouseReleaseEvent(event);

    event->setAccepted(m_containment->containmentActions().contains(Plasma::ContainmentActions::eventToString(event)));
}

void PanelWindow::mousePressEvent(QMouseEvent *event)
{
    if (!event) {
        return;
    }

    QQuickWindow::mousePressEvent(event);

    //even if the menu is executed synchronously, other events may be processed
    //by the qml incubator when plasma is loading, so we need to guard there
    if (m_contextMenu) {
        m_contextMenu.data()->close();
        return;
    }

    const QString trigger = Plasma::ContainmentActions::eventToString(event);
    Plasma::ContainmentActions *plugin = m_containment->containmentActions().value(trigger);

    if (!plugin || plugin->contextualActions().isEmpty()) {
        event->setAccepted(false);
        return;
    }

    //the plugin can be a single action or a context menu
    //Don't have an action list? execute as single action
    //and set the event position as action data
    if (plugin->contextualActions().length() == 1) {
        QAction *action = plugin->contextualActions().at(0);
        action->setData(event->pos());
        action->trigger();
        event->accept();
        return;
    }

    //FIXME: very inefficient appletAt() implementation
    Plasma::Applet *applet = 0;
    foreach (PlasmaQuick::AppletQuickItem *ai, m_appletItems) {
        if (ai && ai->isVisible() && ai->contains(ai->mapFromItem(contentItem(), event->pos()))) {
            applet = ai->applet();
            break;
        } else {
            ai = 0;
        }
    }

    QMenu *desktopMenu = new QMenu;
    desktopMenu->setAttribute(Qt::WA_DeleteOnClose);

    m_contextMenu = desktopMenu;

    if (this->mouseGrabberItem()) {
        //workaround, this fixes for me most of the right click menu behavior
        if (applet) {
            KPluginInfo info = applet->pluginInfo();

            //gives the systemtray direct right click behavior for its applets
            if (info.pluginName() != "org.kde.plasma.systemtray"){
                this->mouseGrabberItem()->ungrabMouse();
            }
        }

        return;
    }

    if (applet) {
        emit applet->contextualActionsAboutToShow();
        addAppletActions(desktopMenu, applet, event);
    } else {
        emit m_containment->contextualActionsAboutToShow();
        addContainmentActions(desktopMenu, event);
    }

    //this is a workaround where Qt now creates the menu widget
    //in .exec before oxygen can polish it and set the following attribute
    desktopMenu->setAttribute(Qt::WA_TranslucentBackground);
    //end workaround

    QPoint pos = event->globalPos();
    if (applet) {
        desktopMenu->adjustSize();

        QRect scr = m_screen->availableGeometry();

        int smallStep = 3;

        int x = event->globalPos().x() + smallStep;
        int y = event->globalPos().y() + smallStep;

        //qDebug()<<x << " - "<<y;

        if (event->globalPos().x() > scr.center().x()) {
            x = event->globalPos().x() - desktopMenu->width() - smallStep;
        }

        if (event->globalPos().y() > scr.center().y()) {
            y = event->globalPos().y() - desktopMenu->height() - smallStep;
        }

        pos = QPoint(x,y);
    }

    if (desktopMenu->isEmpty()) {
        delete desktopMenu;
        event->accept();
        return;
    }

    connect(desktopMenu, SIGNAL(aboutToHide()), this, SLOT(menuAboutToHide()));
    setDisableHiding(true);
    desktopMenu->popup(pos);

    event->setAccepted(true);
}

void PanelWindow::addAppletActions(QMenu *desktopMenu, Plasma::Applet *applet, QEvent *event)
{
    foreach (QAction *action, applet->contextualActions()) {
        if (action) {
            desktopMenu->addAction(action);
        }
    }

    if (!applet->failedToLaunch()) {
        QAction *runAssociatedApplication = applet->actions()->action(QStringLiteral("run associated application"));
        if (runAssociatedApplication && runAssociatedApplication->isEnabled()) {
            desktopMenu->addAction(runAssociatedApplication);
        }

        QAction *configureApplet = applet->actions()->action(QStringLiteral("configure"));
        if (configureApplet && configureApplet->isEnabled()) {
            desktopMenu->addAction(configureApplet);
        }
        QAction *appletAlternatives = applet->actions()->action(QStringLiteral("alternatives"));
        if (appletAlternatives && appletAlternatives->isEnabled()) {
            desktopMenu->addAction(appletAlternatives);
        }
    }

    QMenu *containmentMenu = new QMenu(i18nc("%1 is the name of the containment", "%1 Options", m_containment->title()), desktopMenu);
    addContainmentActions(containmentMenu, event);

    if (!containmentMenu->isEmpty()) {
        int enabled = 0;
        //count number of real actions
        QListIterator<QAction *> actionsIt(containmentMenu->actions());
        while (enabled < 3 && actionsIt.hasNext()) {
            QAction *action = actionsIt.next();
            if (action->isVisible() && !action->isSeparator()) {
                ++enabled;
            }
        }

        if (enabled) {
            //if there is only one, don't create a submenu
            if (enabled < 2) {
                foreach (QAction *action, containmentMenu->actions()) {
                    if (action->isVisible() && !action->isSeparator()) {
                        desktopMenu->addAction(action);
                    }
                }
            } else {
                desktopMenu->addMenu(containmentMenu);
            }
        }
    }

    if (m_containment->immutability() == Plasma::Types::Mutable &&
            (m_containment->containmentType() != Plasma::Types::PanelContainment || m_containment->isUserConfiguring())) {
        QAction *closeApplet = applet->actions()->action(QStringLiteral("remove"));
        //qDebug() << "checking for removal" << closeApplet;
        if (closeApplet) {
            if (!desktopMenu->isEmpty()) {
                desktopMenu->addSeparator();
            }

            //qDebug() << "adding close action" << closeApplet->isEnabled() << closeApplet->isVisible();
            desktopMenu->addAction(closeApplet);
        }
    }
}


void PanelWindow::addContainmentActions(QMenu *desktopMenu, QEvent *event)
{
    if (m_containment->corona()->immutability() != Plasma::Types::Mutable &&
            !KAuthorized::authorizeKAction(QStringLiteral("plasma/containment_actions"))) {
        //qDebug() << "immutability";
        return;
    }

    //this is what ContainmentPrivate::prepareContainmentActions was
    const QString trigger = Plasma::ContainmentActions::eventToString(event);
    Plasma::ContainmentActions *plugin = m_containment->containmentActions().value(trigger);

    if (!plugin) {
        return;
    }

    if (plugin->containment() != m_containment) {
        plugin->setContainment(m_containment);

        // now configure it
        KConfigGroup cfg(m_containment->corona()->config(), "ActionPlugins");
        cfg = KConfigGroup(&cfg, QString::number(m_containment->containmentType()));
        KConfigGroup pluginConfig = KConfigGroup(&cfg, trigger);
        plugin->restore(pluginConfig);
    }

    QList<QAction *> actions = plugin->contextualActions();

    if (actions.isEmpty()) {
        //it probably didn't bother implementing the function. give the user a chance to set
        //a better plugin.  note that if the user sets no-plugin this won't happen...
        if ((m_containment->containmentType() != Plasma::Types::PanelContainment &&
             m_containment->containmentType() != Plasma::Types::CustomPanelContainment) &&
                m_containment->actions()->action(QStringLiteral("configure"))) {
            desktopMenu->addAction(m_containment->actions()->action(QStringLiteral("configure")));
        }
    } else {
        desktopMenu->addActions(actions);
    }

    return;
}


void PanelWindow::screenChanged(QScreen *screen)
{
    if (screen) {
        if (m_screen) {
            disconnect(m_screen, SIGNAL(geometryChanged(const QRect &)), this, SIGNAL(screenGeometryChanged()));
        }

        m_screen = screen;
        connect(m_screen, SIGNAL(geometryChanged(const QRect &)), this, SIGNAL(screenGeometryChanged()));

        emit screenGeometryChanged();
    }
}

void PanelWindow::showEvent(QShowEvent *event)
{
    if ( !m_maskArea.isNull() ) {
        setMask(m_maskArea);
    }
}

} //NowDock namespace
