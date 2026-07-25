#pragma once
#include "../mischelpers_global.h"

#include <QProxyStyle>

#include <QStyledItemDelegate>
class MISCHELPERS_EXPORT CTreeItemDelegate : public QStyledItemDelegate
{
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        QStyleOptionViewItem opt(option);
        opt.state &= ~QStyle::State_HasFocus;
        QStyledItemDelegate::paint(painter, opt, index);
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(18);
        return size;
    }
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Implements properly oriented side tabs
//

class MISCHELPERS_EXPORT CustomTabStyle : public QProxyStyle {
public:
    CustomTabStyle(QStyle* style = 0) : QProxyStyle(style) {}

    QSize sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& size, const QWidget* widget) const {
        QSize s = QProxyStyle::sizeFromContents(type, option, size, widget);
        if (type == QStyle::CT_TabBarTab) {
            //const QTabWidget* pTabWidget = qobject_cast<const QTabWidget*>(widget->parentWidget());
            //if (pTabWidget && pTabWidget->tabPosition() == QTabWidget::West)
            const QTabBar* pTabBar = qobject_cast<const QTabBar*>(widget);
            if(pTabBar && (pTabBar->shape() == QTabBar::RoundedWest || pTabBar->shape() == QTabBar::TriangularWest))
            {
                s.transpose();

                int size = widget->property("imgSize").toInt();
                if (!size) size = 24;
                bool bNoLabels = widget->property("noLabels").toBool();

                QString baseName = baseStyle()->metaObject()->className();
                if (baseName == "QFusionStyle")
                    s.setHeight(s.height() + (bNoLabels ? 18 : 8));
                else
                    s.setHeight(s.height() + (bNoLabels ? 16 : 12));

                if (bNoLabels)
                    s.setWidth(s.height() + 12);
                else
                    s.setWidth(s.width() + size); // for the the icon
            }
        }
        return s;
    }

    void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget) const {
        if (element == CE_TabBarTabLabel) {
            //const QTabWidget* pTabWidget = qobject_cast<const QTabWidget*>(widget->parentWidget());
            //if (pTabWidget && pTabWidget->tabPosition() == QTabWidget::West)
            const QTabBar* pTabBar = qobject_cast<const QTabBar*>(widget);
            if(pTabBar && (pTabBar->shape() == QTabBar::RoundedWest || pTabBar->shape() == QTabBar::TriangularWest)) 
            {
                if (const QStyleOptionTab* tab = qstyleoption_cast<const QStyleOptionTab*>(option)) {
                    QStyleOptionTab opt(*tab);
                    opt.shape = QTabBar::RoundedNorth;

                    int size = widget->property("imgSize").toInt();
                    if (!size) size = 24;
                    bool bNoLabels = widget->property("noLabels").toBool();
                    opt.iconSize = QSize(size, size);
                    if (bNoLabels)
                        opt.text.clear();

                    QProxyStyle::drawControl(element, &opt, painter, widget);
                    return;
                }
            }
        }
        QProxyStyle::drawControl(element, option, painter, widget);
    }
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Keeps submenus visible when the mosue leaves
//

class MISCHELPERS_EXPORT KeepSubMenusVisibleStyle : public QProxyStyle {
public:
    KeepSubMenusVisibleStyle(QStyle* style = 0) : QProxyStyle(style) {}

    int styleHint(StyleHint styleHint, const QStyleOption* opt = nullptr,
        const QWidget* widget = nullptr, QStyleHintReturn* returnData = nullptr) const override
    {
        if (styleHint == SH_Menu_SubMenuDontStartSloppyOnLeave) 
            return 1;
        return QProxyStyle::styleHint(styleHint, opt, widget, returnData);
    }
};


//////////////////////////////////////////////////////////////////////////////////////////////////////
// Implements nice flat buttons
//

class MISCHELPERS_EXPORT MyButtonStyle : public QProxyStyle
{
public:
    MyButtonStyle(QStyle* pStyle) : QProxyStyle(pStyle) {}

    virtual QSize sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& size, const QWidget* widget) const
    {
        QSize s = QProxyStyle::sizeFromContents(type, option, size, widget);
        if (type == CE_PushButton && widget->property("leftButton").toBool())
        {
            if (const QStyleOptionButton* button = qstyleoption_cast<const QStyleOptionButton*>(option))
            {
                if (!button->icon.isNull()) {
                    s.setWidth(s.width() + 20 + button->iconSize.width());
                }
            }
        }
        return s;
    }

    virtual void drawControl(ControlElement element, const QStyleOption* opt, QPainter* p, const QWidget* widget) const 
    {
        if (element == CE_PushButtonLabel && widget->property("leftButton").toBool())
        {
            if (const QStyleOptionButton* button = qstyleoption_cast<const QStyleOptionButton*>(opt))
            {
                QRect textRect = button->rect;
                uint tf = Qt::AlignVCenter | Qt::TextShowMnemonic;
                if (!proxy()->styleHint(SH_UnderlineShortcut, button, widget))
                    tf |= Qt::TextHideMnemonic;

                if (!button->icon.isNull()) {
                    QRect iconRect;
                    QIcon::Mode mode = button->state & State_Enabled ? QIcon::Normal : QIcon::Disabled;
                    if (mode == QIcon::Normal && button->state & State_HasFocus)
                        mode = QIcon::Active;
                    QIcon::State state = QIcon::Off;
                    if (button->state & State_On)
                        state = QIcon::On;

                    QPixmap pixmap = button->icon.pixmap(widget ? widget->window()->windowHandle() : 0, button->iconSize, mode, state);

                    int pixmapWidth = pixmap.width() / pixmap.devicePixelRatio();
                    int pixmapHeight = pixmap.height() / pixmap.devicePixelRatio();
                    int labelWidth = pixmapWidth;
                    int labelHeight = pixmapHeight;
                    int iconSpacing = 4;//### 4 is currently hardcoded in QPushButton::sizeHint()
                    int textWidth = button->fontMetrics.boundingRect(opt->rect, tf, button->text).width();
                    if (!button->text.isEmpty())
                        labelWidth += (textWidth + iconSpacing);

                    textRect.setLeft(textRect.left() + 20 + button->iconSize.width());

                    /*************************************************************/
                    // Make the icon rectangle always be 10px in from the left edge
                    /*************************************************************/
                    iconRect = QRect(10,
                        textRect.y() + (textRect.height() - labelHeight) / 2,
                        pixmapWidth, pixmapHeight);

                    iconRect = visualRect(button->direction, textRect, iconRect);

                    /***********************************/
                    // Always horizontal align the text
                    /***********************************/
                    tf |= Qt::AlignLeft;


                    if (button->state & (State_On | State_Sunken))
                        iconRect.translate(proxy()->pixelMetric(PM_ButtonShiftHorizontal, opt, widget),
                            proxy()->pixelMetric(PM_ButtonShiftVertical, opt, widget));
                    p->drawPixmap(iconRect, pixmap);
                }
                else {
                    tf |= Qt::AlignHCenter;
                }
                if (button->state & (State_On | State_Sunken))
                    textRect.translate(proxy()->pixelMetric(PM_ButtonShiftHorizontal, opt, widget),
                        proxy()->pixelMetric(PM_ButtonShiftVertical, opt, widget));

                if (button->features & QStyleOptionButton::HasMenu) {
                    int indicatorSize = proxy()->pixelMetric(PM_MenuButtonIndicator, button, widget);
                    if (button->direction == Qt::LeftToRight)
                        textRect = textRect.adjusted(0, 0, -indicatorSize, 0);
                    else
                        textRect = textRect.adjusted(indicatorSize, 0, 0, 0);
                }
                proxy()->drawItemText(p, textRect, tf, button->palette, (button->state & State_Enabled),
                    button->text, QPalette::ButtonText);
            }
            return;
        }


        // For all other controls, draw the default
        QProxyStyle::drawControl(element, opt, p, widget);
    }
};
