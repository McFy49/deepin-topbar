#include "itempopupwindow.h"

#include <QRegion>
#include <QScreen>
#include <QApplication>
#include <QDesktopWidget>
#include <QGraphicsOpacityEffect>

DWIDGET_USE_NAMESPACE

ItemPopupWindow::ItemPopupWindow(QWidget *parent)
    : DArrowRectangle(ArrowBottom, parent)
    , m_mouseArea(new XMouseArea("com.deepin.api.XMouseArea", "/com/deepin/api/XMouseArea", QDBusConnection::sessionBus(), this))
{
    m_wmHelper = DWindowManagerHelper::instance();

    compositeChanged();

    setBackgroundColor(DBlurEffectWidget::LightColor);
    setWindowFlags(Qt::X11BypassWindowManagerHint  | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_InputMethodEnabled, false);

    connect(m_wmHelper, &DWindowManagerHelper::hasCompositeChanged, this, &ItemPopupWindow::compositeChanged);

    m_mouseArea->setSync(false);

    QDBusPendingCall call = m_mouseArea->RegisterFullScreen();
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        if (watcher->isError()) {
            qWarning() << "error registering mouse area";
        } else {
            QDBusReply<QString> reply = watcher->reply();
            m_key = reply.value();
        }
    });

    m_moveAni = new QVariantAnimation(this);
    m_moveAni->setDuration(250);
}

ItemPopupWindow::~ItemPopupWindow()
{
}

void ItemPopupWindow::setItemInter(PluginsItemInterface *itemInter)
{
    m_itemInter = itemInter;

    connect(m_mouseArea, &__XMouseArea::ButtonPress, this, [this] (int, int x, int y, const QString &key) {
        if (key == m_key && !containsPoint(QPoint(x, y))) {
            if (isVisible()) {
                m_itemInter->popupHide();
                m_isVisiable = false;
            }
        }
    });
}

void ItemPopupWindow::setContent(QWidget *content)
{
    QWidget *lastWidget = getContent();
    if (lastWidget)
        lastWidget->removeEventFilter(this);
    content->installEventFilter(this);

    m_content = content;

    setAccessibleName(content->objectName() + "-popup");

    DArrowRectangle::setContent(content);
}

void ItemPopupWindow::show(const int x, const int y)
{
    m_moveAni->setStartValue(m_point.isNull() ? QPoint(x - 10, y) : m_point);
    m_moveAni->setEndValue(QPoint(x, y));
    m_point = QPoint(x, y);

    connect(m_moveAni, &QVariantAnimation::valueChanged, this,  [=] (const QVariant &value) {
        move(value.toPoint().x(), value.toPoint().y());
        resizeWithContent();
    });

    m_moveAni->start();

    m_itemInter->popupShow();
}

void ItemPopupWindow::compositeChanged()
{
    if (m_wmHelper->hasComposite())
        setBorderColor(QColor(255, 255, 255, 255 * 0.05));
    else
        setBorderColor(QColor("#2C3238"));
}

bool ItemPopupWindow::containsPoint(const QPoint &point) const
{
    QRect screen = QApplication::desktop()->screenGeometry(QApplication::desktop()->primaryScreen());
    QRect r(screen.x(), screen.y(), screen.width(), 27);

    // if click self;
    QRect self(m_itemInter->itemWidget("")->mapToGlobal(m_itemInter->itemWidget("")->pos()), m_itemInter->itemWidget("")->size());
    if (isVisible() && self.contains(point))
        return false;

    if (r.contains(point) || geometry().contains(point))
        return true;
    return false;
}

bool ItemPopupWindow::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched);

    if (event->type() == QEvent::Resize) {
        DArrowRectangle::resizeWithContent();
    }

    return false;
}
