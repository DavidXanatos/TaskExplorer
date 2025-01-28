/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWidgets module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "private/qlayoutengine_p.h"
#if QT_CONFIG(itemviews)
#include "qabstractitemdelegate.h"
#endif
#include "qapplication.h"
#include "qbitmap.h"
#include "qcursor.h"
#include "qevent.h"
#include "qpainter.h"
#include "qstyle.h"
#include "qstyleoption.h"
#include "qstylepainter.h"
#if QT_CONFIG(tabwidget)
#include "qtabwidget.h"
#endif
#include "qtooltip.h"
#if QT_CONFIG(whatsthis)
#include "qwhatsthis.h"
#endif
#include "private/qtextengine_p.h"
#ifndef QT_NO_ACCESSIBILITY
#include "qaccessible.h"
#endif
#ifdef Q_OS_OSX
#include <qpa/qplatformnativeinterface.h>
#endif

#include "qdebug.h"
#include "qtabbarex_p.h"

#if 0 // Used to be included in Qt4 for Q_WS_MAC
#include <private/qt_mac_p.h>
#include <private/qt_cocoa_helpers_mac_p.h>
#endif

QT_BEGIN_NAMESPACE

QMovableTabWidget::QMovableTabWidget(QWidget *parent)
    : QWidget(parent)
{
}

void QMovableTabWidget::setPixmap(const QPixmap &pixmap)
{
    m_pixmap = pixmap;
    update();
}

void QMovableTabWidget::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);
    QPainter p(this);
    p.drawPixmap(0, 0, m_pixmap);
}

inline static bool verticalTabs(QTabBarEx::Shape shape)
{
    return shape == QTabBarEx::RoundedWest
           || shape == QTabBarEx::RoundedEast
           || shape == QTabBarEx::TriangularWest
           || shape == QTabBarEx::TriangularEast;
}

void QTabBarExPrivate::updateMacBorderMetrics()
{
#if defined(Q_OS_OSX)
    Q_Q(QTabBarEx);
    // Extend the unified title and toolbar area to cover the tab bar iff
    // 1) the tab bar is in document mode
    // 2) the tab bar is directly below an "unified" area.
    // The extending itself is done in the Cocoa platform plugin and Mac style,
    // this function registers geometry and visibility state for the tab bar.

    // Calculate geometry
    int upper, lower;
    if (documentMode) {
        QPoint windowPos = q->mapTo(q->window(), QPoint(0,0));
        upper = windowPos.y();
        int tabStripHeight = q->tabSizeHint(0).height();
        int pixelTweak = -3;
        lower = upper + tabStripHeight + pixelTweak;
    } else {
        upper = 0;
        lower = 0;
    }

    QPlatformNativeInterface *nativeInterface = QGuiApplication::platformNativeInterface();
    quintptr identifier = reinterpret_cast<quintptr>(q);

    // Set geometry
    QPlatformNativeInterface::NativeResourceForIntegrationFunction function =
        nativeInterface->nativeResourceFunctionForIntegration("registerContentBorderArea");
    if (!function)
        return; // Not Cocoa platform plugin.
    typedef void (*RegisterContentBorderAreaFunction)(QWindow *window, quintptr identifier, int upper, int lower);
    (reinterpret_cast<RegisterContentBorderAreaFunction>(function))(q->window()->windowHandle(), identifier, upper, lower);

    // Set visibility state
    function = nativeInterface->nativeResourceFunctionForIntegration("setContentBorderAreaEnabled");
    if (!function)
        return;
    typedef void (*SetContentBorderAreaEnabledFunction)(QWindow *window, quintptr identifier, bool enable);
    (reinterpret_cast<SetContentBorderAreaEnabledFunction>(function))(q->window()->windowHandle(), identifier, q->isVisible());
#endif
}

/*!
    \internal
    This is basically QTabBarEx::initStyleOption() but
    without the expensive QFontMetrics::elidedText() call.
*/

void QTabBarExPrivate::initBasicStyleOption(QStyleOptionTab *option, int tabIndex) const
{
    Q_Q(const QTabBarEx);
    const int totalTabs = tabList.size();

    if (!option || (tabIndex < 0 || tabIndex >= totalTabs))
        return;

    const QTabBarExPrivate::Tab &tab = tabList.at(tabIndex);
    option->initFrom(q);
    option->state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver);
    option->rect = q->tabRect(tabIndex);
    const bool isCurrent = tabIndex == currentIndex;
    option->row = 0;
    if (tabIndex == pressedIndex)
        option->state |= QStyle::State_Sunken;
    if (isCurrent)
        option->state |= QStyle::State_Selected;
    if (isCurrent && q->hasFocus())
        option->state |= QStyle::State_HasFocus;
    if (!tab.enabled)
        option->state &= ~QStyle::State_Enabled;
    if (q->isActiveWindow())
        option->state |= QStyle::State_Active;
    if (!dragInProgress && option->rect == hoverRect)
        option->state |= QStyle::State_MouseOver;
    option->shape = (QTabBar::Shape)shape;
    option->text = tab.text;

    if (tab.textColor.isValid())
        option->palette.setColor(q->foregroundRole(), tab.textColor);
    option->icon = tab.icon;
    option->iconSize = q->iconSize();  // Will get the default value then.

    option->leftButtonSize = tab.leftWidget ? tab.leftWidget->size() : QSize();
    option->rightButtonSize = tab.rightWidget ? tab.rightWidget->size() : QSize();
    option->documentMode = documentMode;

    if (tabIndex > 0 && tabIndex - 1 == currentIndex)
        option->selectedPosition = QStyleOptionTab::PreviousIsSelected;
    else if (tabIndex + 1 < totalTabs && tabIndex + 1 == currentIndex)
        option->selectedPosition = QStyleOptionTab::NextIsSelected;
    else
        option->selectedPosition = QStyleOptionTab::NotAdjacent;

    const bool paintBeginning = (tabIndex == 0) || (dragInProgress && tabIndex == pressedIndex + 1);
    const bool paintEnd = (tabIndex == totalTabs - 1) || (dragInProgress && tabIndex == pressedIndex - 1);
    if (paintBeginning) {
        if (paintEnd)
            option->position = QStyleOptionTab::OnlyOneTab;
        else
            option->position = QStyleOptionTab::Beginning;
    } else if (paintEnd) {
        option->position = QStyleOptionTab::End;
    } else {
        option->position = QStyleOptionTab::Middle;
    }

#if QT_CONFIG(tabwidget)
    if (const QTabWidget *tw = qobject_cast<const QTabWidget *>(q->parentWidget())) {
        option->features |= QStyleOptionTab::HasFrame;
        if (tw->cornerWidget(Qt::TopLeftCorner) || tw->cornerWidget(Qt::BottomLeftCorner))
            option->cornerWidgets |= QStyleOptionTab::LeftCornerWidget;
        if (tw->cornerWidget(Qt::TopRightCorner) || tw->cornerWidget(Qt::BottomRightCorner))
            option->cornerWidgets |= QStyleOptionTab::RightCornerWidget;
    }
#endif
}

/*!
    Initialize \a option with the values from the tab at \a tabIndex. This method
    is useful for subclasses when they need a QStyleOptionTab,
    but don't want to fill in all the information themselves.

    \sa QStyleOption::initFrom(), QTabWidget::initStyleOption()
*/
void QTabBarEx::initStyleOption(QStyleOptionTab *option, int tabIndex) const
{
    Q_D(const QTabBarEx);
    d->initBasicStyleOption(option, tabIndex);

    QRect textRect = style()->subElementRect(QStyle::SE_TabBarTabText, option, this);
    option->text = fontMetrics().elidedText(option->text, d->elideMode, textRect.width(),
                        Qt::TextShowMnemonic);
}

/*!
    \class QTabBarEx
    \brief The QTabBarEx class provides a tab bar, e.g. for use in tabbed dialogs.

    \ingroup basicwidgets
    \inmodule QtWidgets

    QTabBarEx is straightforward to use; it draws the tabs using one of
    the predefined \l{QTabBarEx::Shape}{shapes}, and emits a
    signal when a tab is selected. It can be subclassed to tailor the
    look and feel. Qt also provides a ready-made \l{QTabWidget}.

    Each tab has a tabText(), an optional tabIcon(), an optional
    tabToolTip(), optional tabWhatsThis() and optional tabData().
    The tabs's attributes can be changed with setTabText(), setTabIcon(),
    setTabToolTip(), setTabWhatsThis and setTabData(). Each tabs can be
    enabled or disabled individually with setTabEnabled().

    Each tab can display text in a distinct color. The current text color
    for a tab can be found with the tabTextColor() function. Set the text
    color for a particular tab with setTabTextColor().

    Tabs are added using addTab(), or inserted at particular positions
    using insertTab(). The total number of tabs is given by
    count(). Tabs can be removed from the tab bar with
    removeTab(). Combining removeTab() and insertTab() allows you to
    move tabs to different positions.

    The \l shape property defines the tabs' appearance. The choice of
    shape is a matter of taste, although tab dialogs (for preferences
    and similar) invariably use \l RoundedNorth.
    Tab controls in windows other than dialogs almost
    always use either \l RoundedSouth or \l TriangularSouth. Many
    spreadsheets and other tab controls in which all the pages are
    essentially similar use \l TriangularSouth, whereas \l
    RoundedSouth is used mostly when the pages are different (e.g. a
    multi-page tool palette). The default in QTabBarEx is \l
    RoundedNorth.

    The most important part of QTabBarEx's API is the currentChanged()
    signal.  This is emitted whenever the current tab changes (even at
    startup, when the current tab changes from 'none'). There is also
    a slot, setCurrentIndex(), which can be used to select a tab
    programmatically. The function currentIndex() returns the index of
    the current tab, \l count holds the number of tabs.

    QTabBarEx creates automatic mnemonic keys in the manner of QAbstractButton;
    e.g. if a tab's label is "\&Graphics", Alt+G becomes a shortcut
    key for switching to that tab.

    The following virtual functions may need to be reimplemented in
    order to tailor the look and feel or store extra data with each
    tab:

    \list
    \li tabSizeHint() calcuates the size of a tab.
    \li tabInserted() notifies that a new tab was added.
    \li tabRemoved() notifies that a tab was removed.
    \li tabLayoutChange() notifies that the tabs have been re-laid out.
    \li paintEvent() paints all tabs.
    \endlist

    For subclasses, you might also need the tabRect() functions which
    returns the visual geometry of a single tab.

    \table 100%
    \row \li \inlineimage fusion-tabbar.png Screenshot of a Fusion style tab bar
         \li A tab bar shown in the \l{Qt Widget Gallery}{Fusion widget style}.
    \row \li \inlineimage fusion-tabbar-truncated.png Screenshot of a truncated Fusion tab bar
         \li A truncated tab bar shown in the Fusion widget style.
    \endtable

    \sa QTabWidget
*/

/*!
    \enum QTabBarEx::Shape

    This enum type lists the built-in shapes supported by QTabBarEx. Treat these
    as hints as some styles may not render some of the shapes. However,
    position should be honored.

    \value RoundedNorth  The normal rounded look above the pages

    \value RoundedSouth  The normal rounded look below the pages

    \value RoundedWest  The normal rounded look on the left side of the pages

    \value RoundedEast  The normal rounded look on the right side the pages

    \value TriangularNorth  Triangular tabs above the pages.

    \value TriangularSouth  Triangular tabs similar to those used in
    the Excel spreadsheet, for example

    \value TriangularWest  Triangular tabs on the left of the pages.

    \value TriangularEast  Triangular tabs on the right of the pages.
*/

/*!
    \fn void QTabBarEx::currentChanged(int index)

    This signal is emitted when the tab bar's current tab changes. The
    new current has the given \a index, or -1 if there isn't a new one
    (for example, if there are no tab in the QTabBarEx)
*/

/*!
    \fn void QTabBarEx::tabCloseRequested(int index)
    \since 4.5

    This signal is emitted when the close button on a tab is clicked.
    The \a index is the index that should be removed.

    \sa setTabsClosable()
*/

/*!
    \fn void QTabBarEx::tabMoved(int from, int to)
    \since 4.5

    This signal is emitted when the tab has moved the tab
    at index position \a from to index position \a to.

    note: QTabWidget will automatically move the page when
    this signal is emitted from its tab bar.

    \sa moveTab()
*/

/*!
    \fn void QTabBarEx::tabBarClicked(int index)

    This signal is emitted when user clicks on a tab at an \a index.

    \a index is the index of a clicked tab, or -1 if no tab is under the cursor.

    \since 5.2
*/

/*!
    \fn void QTabBarEx::tabBarDoubleClicked(int index)

    This signal is emitted when the user double clicks on a tab at \a index.

    \a index refers to the tab clicked, or -1 if no tab is under the cursor.

    \since 5.2
*/

void QTabBarExPrivate::init()
{
    Q_Q(QTabBarEx);
    leftB = new QToolButton(q);
    leftB->setAutoRepeat(true);
    QObject::connect(leftB, SIGNAL(clicked()), q, SLOT(_q_scrollTabs()));
    leftB->hide();
    rightB = new QToolButton(q);
    rightB->setAutoRepeat(true);
    QObject::connect(rightB, SIGNAL(clicked()), q, SLOT(_q_scrollTabs()));
    rightB->hide();
#ifdef QT_KEYPAD_NAVIGATION
    if (QApplication::keypadNavigationEnabled()) {
        leftB->setFocusPolicy(Qt::NoFocus);
        rightB->setFocusPolicy(Qt::NoFocus);
        q->setFocusPolicy(Qt::NoFocus);
    } else
#endif
        q->setFocusPolicy(Qt::TabFocus);

#ifndef QT_NO_ACCESSIBILITY
    leftB->setAccessibleName(QTabBarEx::tr("Scroll Left"));
    rightB->setAccessibleName(QTabBarEx::tr("Scroll Right"));
#endif
    q->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    elideMode = Qt::TextElideMode(q->style()->styleHint(QStyle::SH_TabBar_ElideMode, 0, q));
    useScrollButtons = !q->style()->styleHint(QStyle::SH_TabBar_PreferNoArrows, 0, q);
}

QTabBarExPrivate::Tab *QTabBarExPrivate::at(int index)
{
    return validIndex(index)?&tabList[index]:0;
}

const QTabBarExPrivate::Tab *QTabBarExPrivate::at(int index) const
{
    return validIndex(index)?&tabList[index]:0;
}

int QTabBarExPrivate::indexAtPos(const QPoint &p) const
{
    Q_Q(const QTabBarEx);
    if (q->tabRect(currentIndex).contains(p))
        return currentIndex;
    for (int i = 0; i < tabList.count(); ++i)
        if (tabList.at(i).enabled && q->tabRect(i).contains(p))
            return i;
    return -1;
}

void QTabBarExPrivate::layoutTabs()
{
	if(multiRow)
	{
		layoutTabsEx();
		return;
	}

    Q_Q(QTabBarEx);
    layoutDirty = false;
    QSize size = q->size();
    int last, available;
    int maxExtent;
    int i;
    bool vertTabs = verticalTabs(shape);
    int tabChainIndex = 0;

    Qt::Alignment tabAlignment = Qt::Alignment(q->style()->styleHint(QStyle::SH_TabBar_Alignment, 0, q));
    QVector<QLayoutStruct> tabChain(tabList.count() + 2);

    // We put an empty item at the front and back and set its expansive attribute
    // depending on tabAlignment and expanding.
    tabChain[tabChainIndex].init();
    tabChain[tabChainIndex].expansive = (!expanding)
                                        && (tabAlignment != Qt::AlignLeft)
                                        && (tabAlignment != Qt::AlignJustify);
    tabChain[tabChainIndex].empty = true;
    ++tabChainIndex;

    // We now go through our list of tabs and set the minimum size and the size hint
    // This will allow us to elide text if necessary. Since we don't set
    // a maximum size, tabs will EXPAND to fill up the empty space.
    // Since tab widget is rather *ahem* strict about keeping the geometry of the
    // tab bar to its absolute minimum, this won't bleed through, but will show up
    // if you use tab bar on its own (a.k.a. not a bug, but a feature).
    // Update: if expanding is false, we DO set a maximum size to prevent the tabs
    // being wider than necessary.
    if (!vertTabs) {
        int minx = 0;
        int x = 0;
        int maxHeight = 0;
        for (i = 0; i < tabList.count(); ++i, ++tabChainIndex) {
            QSize sz = q->tabSizeHint(i);
            tabList[i].maxRect = QRect(x, 0, sz.width(), sz.height());
            x += sz.width();
            maxHeight = qMax(maxHeight, sz.height());
            sz = q->minimumTabSizeHint(i);
            tabList[i].minRect = QRect(minx, 0, sz.width(), sz.height());
            minx += sz.width();
            tabChain[tabChainIndex].init();
            tabChain[tabChainIndex].sizeHint = tabList.at(i).maxRect.width();
            tabChain[tabChainIndex].minimumSize = sz.width();
            tabChain[tabChainIndex].empty = false;
            tabChain[tabChainIndex].expansive = true;

            if (!expanding)
                tabChain[tabChainIndex].maximumSize = tabChain[tabChainIndex].sizeHint;
        }

        last = minx;
        available = size.width();
        maxExtent = maxHeight;
    } else {
        int miny = 0;
        int y = 0;
        int maxWidth = 0;
        for (i = 0; i < tabList.count(); ++i, ++tabChainIndex) {
            QSize sz = q->tabSizeHint(i);
            tabList[i].maxRect = QRect(0, y, sz.width(), sz.height());
            y += sz.height();
            maxWidth = qMax(maxWidth, sz.width());
            sz = q->minimumTabSizeHint(i);
            tabList[i].minRect = QRect(0, miny, sz.width(), sz.height());
            miny += sz.height();
            tabChain[tabChainIndex].init();
            tabChain[tabChainIndex].sizeHint = tabList.at(i).maxRect.height();
            tabChain[tabChainIndex].minimumSize = sz.height();
            tabChain[tabChainIndex].empty = false;
            tabChain[tabChainIndex].expansive = true;

            if (!expanding)
                tabChain[tabChainIndex].maximumSize = tabChain[tabChainIndex].sizeHint;
        }

        last = miny;
        available = size.height();
        maxExtent = maxWidth;
    }

    // Mirror our front item.
    tabChain[tabChainIndex].init();
    tabChain[tabChainIndex].expansive = (!expanding)
                                        && (tabAlignment != Qt::AlignRight)
                                        && (tabAlignment != Qt::AlignJustify);
    tabChain[tabChainIndex].empty = true;
    Q_ASSERT(tabChainIndex == tabChain.count() - 1); // add an assert just to make sure.

    // Do the calculation
    qGeomCalc(tabChain, 0, tabChain.count(), 0, qMax(available, last), 0);

    // Use the results
    for (i = 0; i < tabList.count(); ++i) {
        const QLayoutStruct &lstruct = tabChain.at(i + 1);
        if (!vertTabs)
            tabList[i].rect.setRect(lstruct.pos, 0, lstruct.size, maxExtent);
        else
            tabList[i].rect.setRect(0, lstruct.pos, maxExtent, lstruct.size);
    }

    if (useScrollButtons && tabList.count() && last > available) {
        const QRect scrollRect = normalizedScrollRect(0);
        scrollOffset = -scrollRect.left();

        Q_Q(QTabBarEx);
        QStyleOption opt;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        opt.init(q);
#else
        opt.initFrom(q);
#endif
        QRect scrollButtonLeftRect = q->style()->subElementRect(QStyle::SE_TabBarScrollLeftButton, &opt, q);
        QRect scrollButtonRightRect = q->style()->subElementRect(QStyle::SE_TabBarScrollRightButton, &opt, q);
        int scrollButtonWidth = q->style()->pixelMetric(QStyle::PM_TabBarScrollButtonWidth, &opt, q);

        // Normally SE_TabBarScrollLeftButton should have the same width as PM_TabBarScrollButtonWidth.
        // But if that is not the case, we set the actual button width to PM_TabBarScrollButtonWidth, and
        // use the extra space from SE_TabBarScrollLeftButton as margins towards the tabs.
        if (vertTabs) {
            scrollButtonLeftRect.setHeight(scrollButtonWidth);
            scrollButtonRightRect.setY(scrollButtonRightRect.bottom() + 1 - scrollButtonWidth);
            scrollButtonRightRect.setHeight(scrollButtonWidth);
            leftB->setArrowType(Qt::UpArrow);
            rightB->setArrowType(Qt::DownArrow);
        } else if (q->layoutDirection() == Qt::RightToLeft) {
            scrollButtonRightRect.setWidth(scrollButtonWidth);
            scrollButtonLeftRect.setX(scrollButtonLeftRect.right() + 1 - scrollButtonWidth);
            scrollButtonLeftRect.setWidth(scrollButtonWidth);
            leftB->setArrowType(Qt::RightArrow);
            rightB->setArrowType(Qt::LeftArrow);
        } else {
            scrollButtonLeftRect.setWidth(scrollButtonWidth);
            scrollButtonRightRect.setX(scrollButtonRightRect.right() + 1 - scrollButtonWidth);
            scrollButtonRightRect.setWidth(scrollButtonWidth);
            leftB->setArrowType(Qt::LeftArrow);
            rightB->setArrowType(Qt::RightArrow);
        }

        leftB->setGeometry(scrollButtonLeftRect);
        leftB->setEnabled(false);
        leftB->show();

        rightB->setGeometry(scrollButtonRightRect);
        rightB->setEnabled(last - scrollOffset > scrollRect.x() + scrollRect.width());
        rightB->show();
    } else {
        scrollOffset = 0;
        rightB->hide();
        leftB->hide();
    }

    layoutWidgets();
    q->tabLayoutChange();
}

void QTabBarExPrivate::layoutTabsEx()
{
    Q_Q(QTabBarEx);
    layoutDirty = false;
    QSize size = q->size();
	int last = 0;
	int available = 0;
    int maxExtent = 0;
    int i;
	int r;
    int tabChainIndex = 0;

    Qt::Alignment tabAlignment = Qt::Alignment(q->style()->styleHint(QStyle::SH_TabBar_Alignment, 0, q));

    QVector<QLayoutStruct> tabChain(tabList.count() + 2);

    // We put an empty item at the front and back and set its expansive attribute
    // depending on tabAlignment and expanding.
    tabChain[tabChainIndex].init();
    tabChain[tabChainIndex].expansive = (!expanding)
                                        && (tabAlignment != Qt::AlignLeft)
                                        && (tabAlignment != Qt::AlignJustify);
    tabChain[tabChainIndex].empty = true;
    ++tabChainIndex;

	QList<int> tabRowStarts;

    // We now go through our list of tabs and set the minimum size and the size hint
    // This will allow us to elide text if necessary. Since we don't set
    // a maximum size, tabs will EXPAND to fill up the empty space.
    // Since tab widget is rather *ahem* strict about keeping the geometry of the
    // tab bar to its absolute minimum, this won't bleed through, but will show up
    // if you use tab bar on its own (a.k.a. not a bug, but a feature).
    // Update: if expanding is false, we DO set a maximum size to prevent the tabs
    // being wider than necessary.

	int ri = 0;

	available = size.width();
	int ypos = 0;
	for (i = 0; i < tabList.count();)
	{
        int xpos = 0;
        int x = 0;
        int maxy = 0;
        for (; i < tabList.count(); ++i, ++tabChainIndex) {
            QSize sz = q->tabSizeHint(i);
			if (xpos > 0 && xpos + sz.width() + 2 > available)
				break;

            tabList[i].maxRect = QRect(x, ypos, sz.width(), sz.height());
            x += sz.width();
            maxy = qMax(maxy, sz.height());
            sz = q->minimumTabSizeHint(i);
            tabList[i].minRect = QRect(xpos, ypos, sz.width(), sz.height());
            xpos += sz.width();
            tabChain[tabChainIndex].init();
            tabChain[tabChainIndex].sizeHint = tabList.at(i).maxRect.width();
            tabChain[tabChainIndex].minimumSize = sz.width();
            tabChain[tabChainIndex].empty = false;
            tabChain[tabChainIndex].expansive = true;

            if (!expanding)
                tabChain[tabChainIndex].maximumSize = tabChain[tabChainIndex].sizeHint;

			if (i == currentIndex)
				ri = tabRowStarts.size();
        }
		tabRowStarts.append(i);

		ypos += maxy;

        last = qMax(xpos, last);
        maxExtent = qMax(maxExtent, maxy);
    }

    // Mirror our front item.
    tabChain[tabChainIndex].init();
    tabChain[tabChainIndex].expansive = (!expanding)
                                        && (tabAlignment != Qt::AlignRight)
                                        && (tabAlignment != Qt::AlignJustify);
    tabChain[tabChainIndex].empty = true;
    Q_ASSERT(tabChainIndex == tabChain.count() - 1); // add an assert just to make sure.

    // Do the calculation
	int prev = 0;
	for (r = 0; r < tabRowStarts.count(); ++r) {
		int cur = tabRowStarts.at(r);

		int start = prev;
		int count = cur + 1 - start;
		if (r + 1 == tabRowStarts.count())
			count += 1;

		qGeomCalc(tabChain, start, count, 0, qMin (available, last), 0);

		prev = cur + 1;
	}

    // Use the results
	prev = 0;
	for (r = 0; r < tabRowStarts.count(); ++r) {
		int cur = tabRowStarts.at(r);

		// make the row with the selected tab most bottom
		int y = maxExtent * ((r + (tabRowStarts.count() - (ri + 1))) % tabRowStarts.count());

		for (i = prev; i < cur; ++i) {
			const QLayoutStruct &lstruct = tabChain.at(i + 1);

			int w = lstruct.size;
			// fix first and last row being 2 pixels to short
			if ((r == 0 || r + 1 == tabRowStarts.count()) && i + 1 == cur)
				w += 2;

			int x = lstruct.pos;
			// fix all rows except the first starting to far to the right
			if (r != 0)
				x += 2;

			int h = maxExtent;
			// fix rows above the most bottin ending to early
			if (r != ri)
				h += 3;

			tabList[i].rect.setRect(x, y, w, h);
		}

		prev = cur;
	}

    scrollOffset = 0;
    rightB->hide();
    leftB->hide();

    layoutWidgets();
    q->tabLayoutChange();

	q->updateGeometry();
    q->update(q->contentsRect());
}

QRect QTabBarExPrivate::normalizedScrollRect(int index)
{
    // "Normalized scroll rect" means return the free space on the tab bar
    // that doesn't overlap with scroll buttons or tear indicators, and
    // always return the rect as horizontal Qt::LeftToRight, even if the
    // tab bar itself is in a different orientation.

    Q_Q(QTabBarEx);
    QStyleOptionTab opt;
    q->initStyleOption(&opt, currentIndex);
    opt.rect = q->rect();

    QRect scrollButtonLeftRect = q->style()->subElementRect(QStyle::SE_TabBarScrollLeftButton, &opt, q);
    QRect scrollButtonRightRect = q->style()->subElementRect(QStyle::SE_TabBarScrollRightButton, &opt, q);
    QRect tearLeftRect = q->style()->subElementRect(QStyle::SE_TabBarTearIndicatorLeft, &opt, q);
    QRect tearRightRect = q->style()->subElementRect(QStyle::SE_TabBarTearIndicatorRight, &opt, q);

    if (verticalTabs(shape)) {
        int topEdge, bottomEdge;
        bool leftButtonIsOnTop = scrollButtonLeftRect.y() < q->height() / 2;
        bool rightButtonIsOnTop = scrollButtonRightRect.y() < q->height() / 2;

        if (leftButtonIsOnTop && rightButtonIsOnTop) {
            topEdge = scrollButtonRightRect.bottom() + 1;
            bottomEdge = q->height();
        } else if (!leftButtonIsOnTop && !rightButtonIsOnTop) {
            topEdge = 0;
            bottomEdge = scrollButtonLeftRect.top();
        } else {
            topEdge = scrollButtonLeftRect.bottom() + 1;
            bottomEdge = scrollButtonRightRect.top();
        }

        bool tearTopVisible = index != 0 && topEdge != -scrollOffset;
        bool tearBottomVisible = index != tabList.size() - 1 && bottomEdge != tabList.constLast().rect.bottom() + 1 - scrollOffset;
        if (tearTopVisible && !tearLeftRect.isNull())
            topEdge = tearLeftRect.bottom() + 1;
        if (tearBottomVisible && !tearRightRect.isNull())
            bottomEdge = tearRightRect.top();

        return QRect(topEdge, 0, bottomEdge - topEdge, q->height());
    } else {
        if (q->layoutDirection() == Qt::RightToLeft) {
            scrollButtonLeftRect = QStyle::visualRect(Qt::RightToLeft, q->rect(), scrollButtonLeftRect);
            scrollButtonRightRect = QStyle::visualRect(Qt::RightToLeft, q->rect(), scrollButtonRightRect);
            tearLeftRect = QStyle::visualRect(Qt::RightToLeft, q->rect(), tearLeftRect);
            tearRightRect = QStyle::visualRect(Qt::RightToLeft, q->rect(), tearRightRect);
        }

        int leftEdge, rightEdge;
        bool leftButtonIsOnLeftSide = scrollButtonLeftRect.x() < q->width() / 2;
        bool rightButtonIsOnLeftSide = scrollButtonRightRect.x() < q->width() / 2;

        if (leftButtonIsOnLeftSide && rightButtonIsOnLeftSide) {
            leftEdge = scrollButtonRightRect.right() + 1;
            rightEdge = q->width();
        } else if (!leftButtonIsOnLeftSide && !rightButtonIsOnLeftSide) {
            leftEdge = 0;
            rightEdge = scrollButtonLeftRect.left();
        } else {
            leftEdge = scrollButtonLeftRect.right() + 1;
            rightEdge = scrollButtonRightRect.left();
        }

        bool tearLeftVisible = index != 0 && leftEdge != -scrollOffset;
        bool tearRightVisible = index != tabList.size() - 1 && rightEdge != tabList.constLast().rect.right() + 1 - scrollOffset;
        if (tearLeftVisible && !tearLeftRect.isNull())
            leftEdge = tearLeftRect.right() + 1;
        if (tearRightVisible && !tearRightRect.isNull())
            rightEdge = tearRightRect.left();

        return QRect(leftEdge, 0, rightEdge - leftEdge, q->height());
    }
}

int QTabBarExPrivate::hoveredTabIndex() const
{
    if (dragInProgress)
        return currentIndex;
    if (hoverIndex >= 0)
        return hoverIndex;
    return -1;
}

void QTabBarExPrivate::makeVisible(int index)
{
    Q_Q(QTabBarEx);
    if (!validIndex(index) || leftB->isHidden())
        return;

    const QRect tabRect = tabList.at(index).rect;
    const int oldScrollOffset = scrollOffset;
    const bool horiz = !verticalTabs(shape);
    const int tabStart = horiz ? tabRect.left() : tabRect.top();
    const int tabEnd = horiz ? tabRect.right() : tabRect.bottom();
    const int lastTabEnd = horiz ? tabList.constLast().rect.right() : tabList.constLast().rect.bottom();
    const QRect scrollRect = normalizedScrollRect(index);
    const int scrolledTabBarStart = qMax(1, scrollRect.left() + scrollOffset);
    const int scrolledTabBarEnd = qMin(lastTabEnd - 1, scrollRect.right() + scrollOffset);

    if (tabStart < scrolledTabBarStart) {
        // Tab is outside on the left, so scroll left.
        scrollOffset = tabStart - scrollRect.left();
    } else if (tabEnd > scrolledTabBarEnd) {
        // Tab is outside on the right, so scroll right.
        scrollOffset = tabEnd - scrollRect.right();
    }

    leftB->setEnabled(scrollOffset > -scrollRect.left());
    rightB->setEnabled(scrollOffset < lastTabEnd - scrollRect.right());

    if (oldScrollOffset != scrollOffset) {
        q->update();
        layoutWidgets();
    }
}

void QTabBarExPrivate::killSwitchTabTimer()
{
    Q_Q(QTabBarEx);
    if (switchTabTimerId) {
        q->killTimer(switchTabTimerId);
        switchTabTimerId = 0;
    }
    switchTabCurrentIndex = -1;
}

void QTabBarExPrivate::layoutTab(int index)
{
    Q_Q(QTabBarEx);
    Q_ASSERT(index >= 0);

    Tab &tab = tabList[index];
    bool vertical = verticalTabs(shape);
    if (!(tab.leftWidget || tab.rightWidget))
        return;

    QStyleOptionTab opt;
    q->initStyleOption(&opt, index);
    if (tab.leftWidget) {
        QRect rect = q->style()->subElementRect(QStyle::SE_TabBarTabLeftButton, &opt, q);
        QPoint p = rect.topLeft();
        if ((index == pressedIndex) || paintWithOffsets) {
            if (vertical)
                p.setY(p.y() + tabList[index].dragOffset);
            else
                p.setX(p.x() + tabList[index].dragOffset);
        }
        tab.leftWidget->move(p);
    }
    if (tab.rightWidget) {
        QRect rect = q->style()->subElementRect(QStyle::SE_TabBarTabRightButton, &opt, q);
        QPoint p = rect.topLeft();
        if ((index == pressedIndex) || paintWithOffsets) {
            if (vertical)
                p.setY(p.y() + tab.dragOffset);
            else
                p.setX(p.x() + tab.dragOffset);
        }
        tab.rightWidget->move(p);
    }
}

void QTabBarExPrivate::layoutWidgets(int start)
{
    Q_Q(QTabBarEx);
    for (int i = start; i < q->count(); ++i) {
        layoutTab(i);
    }
}

void QTabBarExPrivate::autoHideTabs()
{
    Q_Q(QTabBarEx);

    if (autoHide)
        q->setVisible(q->count() > 1);
}

void QTabBarExPrivate::_q_closeTab()
{
    Q_Q(QTabBarEx);
    QObject *object = q->sender();
    int tabToClose = -1;
    QTabBarEx::ButtonPosition closeSide = (QTabBarEx::ButtonPosition)q->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, 0, q);
    for (int i = 0; i < tabList.count(); ++i) {
        if (closeSide == QTabBarEx::LeftSide) {
            if (tabList.at(i).leftWidget == object) {
                tabToClose = i;
                break;
            }
        } else {
            if (tabList.at(i).rightWidget == object) {
                tabToClose = i;
                break;
            }
        }
    }
    if (tabToClose != -1)
        emit q->tabCloseRequested(tabToClose);
}

void QTabBarExPrivate::_q_scrollTabs()
{
    Q_Q(QTabBarEx);
    const QObject *sender = q->sender();
    const bool horizontal = !verticalTabs(shape);
    const QRect scrollRect = normalizedScrollRect();
    int i = -1;

    if (sender == leftB) {
        for (i = tabList.count() - 1; i >= 0; --i) {
            int start = horizontal ? tabList.at(i).rect.left() : tabList.at(i).rect.top();
            if (start < scrollRect.left() + scrollOffset) {
                makeVisible(i);
                return;
            }
        }
    } else if (sender == rightB) {
        for (i = 0; i < tabList.count(); ++i) {
            int end = horizontal ? tabList.at(i).rect.right() : tabList.at(i).rect.bottom();
            if (end > scrollRect.right() + scrollOffset) {
                makeVisible(i);
                return;
            }
        }
    }
}

void QTabBarExPrivate::refresh()
{
    Q_Q(QTabBarEx);

    // be safe in case a subclass is also handling move with the tabs
    if (pressedIndex != -1
        && movable
        && QApplication::mouseButtons() == Qt::NoButton) {
        moveTabFinished(pressedIndex);
        if (!validIndex(pressedIndex))
            pressedIndex = -1;
    }

    if (!q->isVisible()) {
        layoutDirty = true;
    } else {
        layoutTabs();
        makeVisible(currentIndex);
        q->update();
        q->updateGeometry();
    }
}

/*!
    Creates a new tab bar with the given \a parent.
*/
QTabBarEx::QTabBarEx(QWidget* parent)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    :QWidget(*new QTabBarExPrivate, parent, 0)
#else
    :QWidget(*new QTabBarExPrivate, parent, {})
#endif
{
    Q_D(QTabBarEx);
    d->init();
}


/*!
    Destroys the tab bar.
*/
QTabBarEx::~QTabBarEx()
{
}

/*!
    \property QTabBarEx::shape
    \brief the shape of the tabs in the tab bar

    Possible values for this property are described by the Shape enum.
*/


QTabBarEx::Shape QTabBarEx::shape() const
{
    Q_D(const QTabBarEx);
    return d->shape;
}

void QTabBarEx::setShape(QTabBarEx::Shape shape)
{
    Q_D(QTabBarEx);
    if (d->shape == shape)
        return;
    d->shape = shape;
    d->refresh();
}

/*!
    \property QTabBarEx::drawBase
    \brief defines whether or not tab bar should draw its base.

    If true then QTabBarEx draws a base in relation to the styles overlab.
    Otherwise only the tabs are drawn.

    \sa QStyle::pixelMetric(), QStyle::PM_TabBarBaseOverlap, QStyleOptionTabBarBase
*/

void QTabBarEx::setDrawBase(bool drawBase)
{
    Q_D(QTabBarEx);
    if (d->drawBase == drawBase)
        return;
    d->drawBase = drawBase;
    update();
}

bool QTabBarEx::drawBase() const
{
    Q_D(const QTabBarEx);
    return d->drawBase;
}

/*!
    Adds a new tab with text \a text. Returns the new
    tab's index.
*/
int QTabBarEx::addTab(const QString &text)
{
    return insertTab(-1, text);
}

/*!
    \overload

    Adds a new tab with icon \a icon and text \a
    text. Returns the new tab's index.
*/
int QTabBarEx::addTab(const QIcon& icon, const QString &text)
{
    return insertTab(-1, icon, text);
}

/*!
    Inserts a new tab with text \a text at position \a index. If \a
    index is out of range, the new tab is appened. Returns the new
    tab's index.
*/
int QTabBarEx::insertTab(int index, const QString &text)
{
    return insertTab(index, QIcon(), text);
}

/*!\overload

    Inserts a new tab with icon \a icon and text \a text at position
    \a index. If \a index is out of range, the new tab is
    appended. Returns the new tab's index.

    If the QTabBarEx was empty before this function is called, the inserted tab
    becomes the current tab.

    Inserting a new tab at an index less than or equal to the current index
    will increment the current index, but keep the current tab.
*/
int QTabBarEx::insertTab(int index, const QIcon& icon, const QString &text)
{
    Q_D(QTabBarEx);
    if (!d->validIndex(index)) {
        index = d->tabList.count();
        d->tabList.append(QTabBarExPrivate::Tab(icon, text));
    } else {
        d->tabList.insert(index, QTabBarExPrivate::Tab(icon, text));
    }
#ifndef QT_NO_SHORTCUT
    d->tabList[index].shortcutId = grabShortcut(QKeySequence::mnemonic(text));
#endif
    d->refresh();
    if (d->tabList.count() == 1)
        setCurrentIndex(index);
    else if (index <= d->currentIndex)
        ++d->currentIndex;

    if (d->closeButtonOnTabs) {
        QStyleOptionTab opt;
        initStyleOption(&opt, index);
        ButtonPosition closeSide = (ButtonPosition)style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, 0, this);
        QAbstractButton *closeButton = new CloseButton(this);
        connect(closeButton, SIGNAL(clicked()), this, SLOT(_q_closeTab()));
        setTabButton(index, closeSide, closeButton);
    }

    for (int i = 0; i < d->tabList.count(); ++i) {
        if (d->tabList[i].lastTab >= index)
            ++d->tabList[i].lastTab;
    }

    tabInserted(index);
    d->autoHideTabs();
    return index;
}


/*!
    Removes the tab at position \a index.

    \sa SelectionBehavior
 */
void QTabBarEx::removeTab(int index)
{
    Q_D(QTabBarEx);
    if (d->validIndex(index)) {
        if (d->dragInProgress)
            d->moveTabFinished(d->pressedIndex);

#ifndef QT_NO_SHORTCUT
        releaseShortcut(d->tabList.at(index).shortcutId);
#endif
        if (d->tabList[index].leftWidget) {
            d->tabList[index].leftWidget->hide();
            d->tabList[index].leftWidget->deleteLater();
            d->tabList[index].leftWidget = 0;
        }
        if (d->tabList[index].rightWidget) {
            d->tabList[index].rightWidget->hide();
            d->tabList[index].rightWidget->deleteLater();
            d->tabList[index].rightWidget = 0;
        }

        int newIndex = d->tabList[index].lastTab;
        d->tabList.removeAt(index);
        for (int i = 0; i < d->tabList.count(); ++i) {
            if (d->tabList[i].lastTab == index)
                d->tabList[i].lastTab = -1;
            if (d->tabList[i].lastTab > index)
                --d->tabList[i].lastTab;
        }
        if (index == d->currentIndex) {
            // The current tab is going away, in order to make sure
            // we emit that "current has changed", we need to reset this
            // around.
            d->currentIndex = -1;
            if (d->tabList.size() > 0) {
                switch(d->selectionBehaviorOnRemove) {
                case SelectPreviousTab:
                    if (newIndex > index)
                        newIndex--;
                    if (d->validIndex(newIndex))
                        break;
                    Q_FALLTHROUGH();
                case SelectRightTab:
                    newIndex = index;
                    if (newIndex >= d->tabList.size())
                        newIndex = d->tabList.size() - 1;
                    break;
                case SelectLeftTab:
                    newIndex = index - 1;
                    if (newIndex < 0)
                        newIndex = 0;
                    break;
                default:
                    break;
                }

                if (d->validIndex(newIndex)) {
                    // don't loose newIndex's old through setCurrentIndex
                    int bump = d->tabList[newIndex].lastTab;
                    setCurrentIndex(newIndex);
                    d->tabList[newIndex].lastTab = bump;
                }
            } else {
                emit currentChanged(-1);
            }
        } else if (index < d->currentIndex) {
            setCurrentIndex(d->currentIndex - 1);
        }
        d->refresh();
        d->autoHideTabs();
        if (!d->hoverRect.isEmpty()) {
            for (int i = 0; i < d->tabList.count(); ++i) {
                const QRect area = tabRect(i);
                if (area.contains(mapFromGlobal(QCursor::pos()))) {
                    d->hoverIndex = i;
                    d->hoverRect = area;
                    break;
                }
            }
            update(d->hoverRect);
        }
        tabRemoved(index);
    }
}


/*!
    Returns \c true if the tab at position \a index is enabled; otherwise
    returns \c false.
*/
bool QTabBarEx::isTabEnabled(int index) const
{
    Q_D(const QTabBarEx);
    if (const QTabBarExPrivate::Tab *tab = d->at(index))
        return tab->enabled;
    return false;
}

/*!
    If \a enabled is true then the tab at position \a index is
    enabled; otherwise the item at position \a index is disabled.
*/
void QTabBarEx::setTabEnabled(int index, bool enabled)
{
    Q_D(QTabBarEx);
    if (QTabBarExPrivate::Tab *tab = d->at(index)) {
        tab->enabled = enabled;
#ifndef QT_NO_SHORTCUT
        setShortcutEnabled(tab->shortcutId, enabled);
#endif
        update();
        if (!enabled && index == d->currentIndex)
            setCurrentIndex(d->validIndex(index+1)?index+1:0);
        else if (enabled && !d->validIndex(d->currentIndex))
            setCurrentIndex(index);
    }
}


/*!
    Returns the text of the tab at position \a index, or an empty
    string if \a index is out of range.
*/
QString QTabBarEx::tabText(int index) const
{
    Q_D(const QTabBarEx);
    if (const QTabBarExPrivate::Tab *tab = d->at(index))
        return tab->text;
    return QString();
}

/*!
    Sets the text of the tab at position \a index to \a text.
*/
void QTabBarEx::setTabText(int index, const QString &text)
{
    Q_D(QTabBarEx);
    if (QTabBarExPrivate::Tab *tab = d->at(index)) {
        d->textSizes.remove(tab->text);
        tab->text = text;
#ifndef QT_NO_SHORTCUT
        releaseShortcut(tab->shortcutId);
        tab->shortcutId = grabShortcut(QKeySequence::mnemonic(text));
        setShortcutEnabled(tab->shortcutId, tab->enabled);
#endif
        d->refresh();
    }
}

/*!
    Returns the text color of the tab with the given \a index, or a invalid
    color if \a index is out of range.

    \sa setTabTextColor()
*/
QColor QTabBarEx::tabTextColor(int index) const
{
    Q_D(const QTabBarEx);
    if (const QTabBarExPrivate::Tab *tab = d->at(index))
        return tab->textColor;
    return QColor();
}

/*!
    Sets the color of the text in the tab with the given \a index to the specified \a color.

    If an invalid color is specified, the tab will use the QTabBarEx foreground role instead.

    \sa tabTextColor()
*/
void QTabBarEx::setTabTextColor(int index, const QColor &color)
{
    Q_D(QTabBarEx);
    if (QTabBarExPrivate::Tab *tab = d->at(index)) {
        tab->textColor = color;
        update(tabRect(index));
    }
}

/*!
    Returns the icon of the tab at position \a index, or a null icon
    if \a index is out of range.
*/
QIcon QTabBarEx::tabIcon(int index) const
{
    Q_D(const QTabBarEx);
    if (const QTabBarExPrivate::Tab *tab = d->at(index))
        return tab->icon;
    return QIcon();
}

/*!
    Sets the icon of the tab at position \a index to \a icon.
*/
void QTabBarEx::setTabIcon(int index, const QIcon & icon)
{
    Q_D(QTabBarEx);
    if (QTabBarExPrivate::Tab *tab = d->at(index)) {
        bool simpleIconChange = (!icon.isNull() && !tab->icon.isNull());
        tab->icon = icon;
        if (simpleIconChange)
            update(tabRect(index));
        else
            d->refresh();
    }
}

#ifndef QT_NO_TOOLTIP
/*!
    Sets the tool tip of the tab at position \a index to \a tip.
*/
void QTabBarEx::setTabToolTip(int index, const QString & tip)
{
    Q_D(QTabBarEx);
    if (QTabBarExPrivate::Tab *tab = d->at(index))
        tab->toolTip = tip;
}

/*!
    Returns the tool tip of the tab at position \a index, or an empty
    string if \a index is out of range.
*/
QString QTabBarEx::tabToolTip(int index) const
{
    Q_D(const QTabBarEx);
    if (const QTabBarExPrivate::Tab *tab = d->at(index))
        return tab->toolTip;
    return QString();
}
#endif // QT_NO_TOOLTIP

#if QT_CONFIG(whatsthis)
/*!
    \since 4.1

    Sets the What's This help text of the tab at position \a index
    to \a text.
*/
void QTabBarEx::setTabWhatsThis(int index, const QString &text)
{
    Q_D(QTabBarEx);
    if (QTabBarExPrivate::Tab *tab = d->at(index))
        tab->whatsThis = text;
}

/*!
    \since 4.1

    Returns the What's This help text of the tab at position \a index,
    or an empty string if \a index is out of range.
*/
QString QTabBarEx::tabWhatsThis(int index) const
{
    Q_D(const QTabBarEx);
    if (const QTabBarExPrivate::Tab *tab = d->at(index))
        return tab->whatsThis;
    return QString();
}

#endif // QT_CONFIG(whatsthis)

/*!
    Sets the data of the tab at position \a index to \a data.
*/
void QTabBarEx::setTabData(int index, const QVariant & data)
{
    Q_D(QTabBarEx);
    if (QTabBarExPrivate::Tab *tab = d->at(index))
        tab->data = data;
}

/*!
    Returns the data of the tab at position \a index, or a null
    variant if \a index is out of range.
*/
QVariant QTabBarEx::tabData(int index) const
{
    Q_D(const QTabBarEx);
    if (const QTabBarExPrivate::Tab *tab = d->at(index))
        return tab->data;
    return QVariant();
}

/*!
    Returns the visual rectangle of the tab at position \a
    index, or a null rectangle if \a index is out of range.
*/
QRect QTabBarEx::tabRect(int index) const
{
    Q_D(const QTabBarEx);
    if (const QTabBarExPrivate::Tab *tab = d->at(index)) {
        if (d->layoutDirty)
            const_cast<QTabBarExPrivate*>(d)->layoutTabs();
        QRect r = tab->rect;
        if (verticalTabs(d->shape))
            r.translate(0, -d->scrollOffset);
        else
            r.translate(-d->scrollOffset, 0);
        if (!verticalTabs(d->shape))
            r = QStyle::visualRect(layoutDirection(), rect(), r);
        return r;
    }
    return QRect();
}

/*!
    \since 4.3
    Returns the index of the tab that covers \a position or -1 if no
    tab covers \a position;
*/

int QTabBarEx::tabAt(const QPoint &position) const
{
    Q_D(const QTabBarEx);
    if (d->validIndex(d->currentIndex)
        && tabRect(d->currentIndex).contains(position)) {
        return d->currentIndex;
    }
    const int max = d->tabList.size();
    for (int i = 0; i < max; ++i) {
        if (tabRect(i).contains(position)) {
            return i;
        }
    }
    return -1;
}

/*!
    \property QTabBarEx::currentIndex
    \brief the index of the tab bar's visible tab

    The current index is -1 if there is no current tab.
*/

int QTabBarEx::currentIndex() const
{
    Q_D(const QTabBarEx);
    if (d->validIndex(d->currentIndex))
        return d->currentIndex;
    return -1;
}


void QTabBarEx::setCurrentIndex(int index)
{
    Q_D(QTabBarEx);
    if (d->dragInProgress && d->pressedIndex != -1)
        return;

    int oldIndex = d->currentIndex;
    if (d->validIndex(index) && d->currentIndex != index) {
        d->currentIndex = index;
        update();
        d->makeVisible(index);
		if(d->multiRow)
			d->layoutTabsEx();
        d->tabList[index].lastTab = oldIndex;
        if (oldIndex >= 0 && oldIndex < count())
            d->layoutTab(oldIndex);
        d->layoutTab(index);
#ifndef QT_NO_ACCESSIBILITY
        if (QAccessible::isActive()) {
            if (hasFocus()) {
                QAccessibleEvent focusEvent(this, QAccessible::Focus);
                focusEvent.setChild(index);
                QAccessible::updateAccessibility(&focusEvent);
            }
            QAccessibleEvent selectionEvent(this, QAccessible::Selection);
            selectionEvent.setChild(index);
            QAccessible::updateAccessibility(&selectionEvent);
        }
#endif
        emit currentChanged(index);
    }
}

/*!
    \property QTabBarEx::iconSize
    \brief The size for icons in the tab bar
    \since 4.1

    The default value is style-dependent. \c iconSize is a maximum
    size; icons that are smaller are not scaled up.

    \sa QTabWidget::iconSize
*/
QSize QTabBarEx::iconSize() const
{
    Q_D(const QTabBarEx);
    if (d->iconSize.isValid())
        return d->iconSize;
    int iconExtent = style()->pixelMetric(QStyle::PM_TabBarIconSize, 0, this);
    return QSize(iconExtent, iconExtent);

}

void QTabBarEx::setIconSize(const QSize &size)
{
    Q_D(QTabBarEx);
    d->iconSize = size;
    d->layoutDirty = true;
    update();
    updateGeometry();
}

/*!
    \property QTabBarEx::count
    \brief the number of tabs in the tab bar
*/

int QTabBarEx::count() const
{
    Q_D(const QTabBarEx);
    return d->tabList.count();
}


/*!\reimp
 */
QSize QTabBarEx::sizeHint() const
{
    Q_D(const QTabBarEx);
    if (d->layoutDirty)
        const_cast<QTabBarExPrivate*>(d)->layoutTabs();
    QRect r;
    for (int i = 0; i < d->tabList.count(); ++i)
        r = r.united(d->tabList.at(i).maxRect);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QSize sz = QApplication::globalStrut();
    return r.size().expandedTo(sz);
#else
    return r.size();
#endif
}

/*!\reimp
 */
QSize QTabBarEx::minimumSizeHint() const
{
    Q_D(const QTabBarEx);
    if (d->layoutDirty)
        const_cast<QTabBarExPrivate*>(d)->layoutTabs();
    if (!d->useScrollButtons) {
        QRect r;
        for (int i = 0; i < d->tabList.count(); ++i)
            r = r.united(d->tabList.at(i).minRect);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        return r.size().expandedTo(QApplication::globalStrut());
#else
        return r.size();
#endif
    }
    if (verticalTabs(d->shape))
        return QSize(sizeHint().width(), d->rightB->sizeHint().height() * 2 + 75);
    else
        return QSize(d->rightB->sizeHint().width() * 2 + 75, sizeHint().height());
}

// Compute the most-elided possible text, for minimumSizeHint
static QString computeElidedText(Qt::TextElideMode mode, const QString &text)
{
    if (text.length() <= 3)
        return text;

    static const QLatin1String Ellipses("...");
    QString ret;
    switch (mode) {
    case Qt::ElideRight:
        ret = text.left(2) + Ellipses;
        break;
    case Qt::ElideMiddle:
        ret = text.left(1) + Ellipses + text.right(1);
        break;
    case Qt::ElideLeft:
        ret = Ellipses + text.right(2);
        break;
    case Qt::ElideNone:
        ret = text;
        break;
    }
    return ret;
}

/*!
    Returns the minimum tab size hint for the tab at position \a index.
    \since 5.0
*/

QSize QTabBarEx::minimumTabSizeHint(int index) const
{
    Q_D(const QTabBarEx);
    QTabBarExPrivate::Tab &tab = const_cast<QTabBarExPrivate::Tab&>(d->tabList[index]);
    QString oldText = tab.text;
    tab.text = computeElidedText(d->elideMode, oldText);
    QSize size = tabSizeHint(index);
    tab.text = oldText;
    return size;
}

/*!
    Returns the size hint for the tab at position \a index.
*/
QSize QTabBarEx::tabSizeHint(int index) const
{
    //Note: this must match with the computations in QCommonStylePrivate::tabLayout
    Q_D(const QTabBarEx);
    if (const QTabBarExPrivate::Tab *tab = d->at(index)) {
        QStyleOptionTab opt;
        d->initBasicStyleOption(&opt, index);
        opt.text = d->tabList.at(index).text;
        QSize iconSize = tab->icon.isNull() ? QSize(0, 0) : opt.iconSize;
        int hframe = style()->pixelMetric(QStyle::PM_TabBarTabHSpace, &opt, this);
        int vframe = style()->pixelMetric(QStyle::PM_TabBarTabVSpace, &opt, this);
        const QFontMetrics fm = fontMetrics();

        int maxWidgetHeight = qMax(opt.leftButtonSize.height(), opt.rightButtonSize.height());
        int maxWidgetWidth = qMax(opt.leftButtonSize.width(), opt.rightButtonSize.width());

        int widgetWidth = 0;
        int widgetHeight = 0;
        int padding = 0;
        if (!opt.leftButtonSize.isEmpty()) {
            padding += 4;
            widgetWidth += opt.leftButtonSize.width();
            widgetHeight += opt.leftButtonSize.height();
        }
        if (!opt.rightButtonSize.isEmpty()) {
            padding += 4;
            widgetWidth += opt.rightButtonSize.width();
            widgetHeight += opt.rightButtonSize.height();
        }
        if (!opt.icon.isNull())
            padding += 4;

        QHash<QString, QSize>::iterator it = d->textSizes.find(tab->text);
        if (it == d->textSizes.end())
           it = d->textSizes.insert(tab->text, fm.size(Qt::TextShowMnemonic, tab->text));
        const int textWidth = it.value().width();
        QSize csz;
        if (verticalTabs(d->shape)) {
            csz = QSize( qMax(maxWidgetWidth, qMax(fm.height(), iconSize.height())) + vframe,
                         textWidth + iconSize.width() + hframe + widgetHeight + padding);
        } else {
            csz = QSize(textWidth + iconSize.width() + hframe + widgetWidth + padding,
                  qMax(maxWidgetHeight, qMax(fm.height(), iconSize.height())) + vframe);
        }

        QSize retSize = style()->sizeFromContents(QStyle::CT_TabBarTab, &opt, csz, this);
        return retSize;
    }
    return QSize();
}

/*!
  This virtual handler is called after a new tab was added or
  inserted at position \a index.

  \sa tabRemoved()
 */
void QTabBarEx::tabInserted(int index)
{
    Q_UNUSED(index)
}

/*!
  This virtual handler is called after a tab was removed from
  position \a index.

  \sa tabInserted()
 */
void QTabBarEx::tabRemoved(int index)
{
    Q_UNUSED(index)
}

/*!
  This virtual handler is called whenever the tab layout changes.

  \sa tabRect()
 */
void QTabBarEx::tabLayoutChange()
{
}


/*!\reimp
 */
void QTabBarEx::showEvent(QShowEvent *)
{
    Q_D(QTabBarEx);
    if (d->layoutDirty)
        d->refresh();
    if (!d->validIndex(d->currentIndex))
        setCurrentIndex(0);
    d->updateMacBorderMetrics();
}

/*!\reimp
 */
void QTabBarEx::hideEvent(QHideEvent *)
{
    Q_D(QTabBarEx);
    d->updateMacBorderMetrics();
}

/*!\reimp
 */
bool QTabBarEx::event(QEvent *event)
{
    Q_D(QTabBarEx);
    if (event->type() == QEvent::HoverMove
        || event->type() == QEvent::HoverEnter) {
        QHoverEvent *he = static_cast<QHoverEvent *>(event);
        if (!d->hoverRect.contains(he->pos())) {
            QRect oldHoverRect = d->hoverRect;
            bool cursorOverTabs = false;
            for (int i = 0; i < d->tabList.count(); ++i) {
                QRect area = tabRect(i);
                if (area.contains(he->pos())) {
                    d->hoverIndex = i;
                    d->hoverRect = area;
                    cursorOverTabs = true;
                    break;
                }
            }
            if (!cursorOverTabs) {
                d->hoverIndex = -1;
                d->hoverRect = QRect();
            }
            if (he->oldPos() != QPoint(-1, -1))
                update(oldHoverRect);
            update(d->hoverRect);
        }
        return true;
    } else if (event->type() == QEvent::HoverLeave) {
        QRect oldHoverRect = d->hoverRect;
        d->hoverIndex = -1;
        d->hoverRect = QRect();
        update(oldHoverRect);
        return true;
#ifndef QT_NO_TOOLTIP
    } else if (event->type() == QEvent::ToolTip) {
        if (const QTabBarExPrivate::Tab *tab = d->at(tabAt(static_cast<QHelpEvent*>(event)->pos()))) {
            if (!tab->toolTip.isEmpty()) {
                QToolTip::showText(static_cast<QHelpEvent*>(event)->globalPos(), tab->toolTip, this);
                return true;
            }
        }
#endif // QT_NO_TOOLTIP
#if QT_CONFIG(whatsthis)
    } else if (event->type() == QEvent::QueryWhatsThis) {
        const QTabBarExPrivate::Tab *tab = d->at(d->indexAtPos(static_cast<QHelpEvent*>(event)->pos()));
        if (!tab || tab->whatsThis.isEmpty())
            event->ignore();
        return true;
    } else if (event->type() == QEvent::WhatsThis) {
        if (const QTabBarExPrivate::Tab *tab = d->at(d->indexAtPos(static_cast<QHelpEvent*>(event)->pos()))) {
            if (!tab->whatsThis.isEmpty()) {
                QWhatsThis::showText(static_cast<QHelpEvent*>(event)->globalPos(),
                                     tab->whatsThis, this);
                return true;
            }
        }
#endif // QT_CONFIG(whatsthis)
#ifndef QT_NO_SHORTCUT
    } else if (event->type() == QEvent::Shortcut) {
        QShortcutEvent *se = static_cast<QShortcutEvent *>(event);
        for (int i = 0; i < d->tabList.count(); ++i) {
            const QTabBarExPrivate::Tab *tab = &d->tabList.at(i);
            if (tab->shortcutId == se->shortcutId()) {
                setCurrentIndex(i);
                return true;
            }
        }
#endif
    } else if (event->type() == QEvent::MouseButtonDblClick) { // ### fixme Qt 6: move to mouseDoubleClickEvent(), here for BC reasons.
        const QPoint pos = static_cast<const QMouseEvent *>(event)->pos();
        const bool isEventInCornerButtons = (!d->leftB->isHidden() && d->leftB->geometry().contains(pos))
                                            || (!d->rightB->isHidden() && d->rightB->geometry().contains(pos));
        if (!isEventInCornerButtons)
            emit tabBarDoubleClicked(tabAt(pos));
    } else if (event->type() == QEvent::Move) {
        d->updateMacBorderMetrics();
        return QWidget::event(event);

#if QT_CONFIG(draganddrop)
    } else if (event->type() == QEvent::DragEnter) {
        if (d->changeCurrentOnDrag)
            event->accept();
    } else if (event->type() == QEvent::DragMove) {
        if (d->changeCurrentOnDrag) {
            const int tabIndex = tabAt(static_cast<QDragMoveEvent *>(event)->pos());
            if (isTabEnabled(tabIndex) && d->switchTabCurrentIndex != tabIndex) {
                d->switchTabCurrentIndex = tabIndex;
                if (d->switchTabTimerId)
                    killTimer(d->switchTabTimerId);
                d->switchTabTimerId = startTimer(style()->styleHint(QStyle::SH_TabBar_ChangeCurrentDelay));
            }
            event->ignore();
        }
    } else if (event->type() == QEvent::DragLeave || event->type() == QEvent::Drop) {
        d->killSwitchTabTimer();
        event->ignore();
#endif
    }
    return QWidget::event(event);
}

/*!\reimp
 */
void QTabBarEx::resizeEvent(QResizeEvent *)
{
    Q_D(QTabBarEx);
    if (d->layoutDirty)
        updateGeometry();
    d->layoutTabs();

    d->makeVisible(d->currentIndex);
}

/*!\reimp
 */
void QTabBarEx::paintEvent(QPaintEvent *)
{
    Q_D(QTabBarEx);

    QStyleOptionTabBarBase optTabBase;
    QTabBarExPrivate::initStyleBaseOption(&optTabBase, this, size());

    QStylePainter p(this);
    int selected = -1;
    int cutLeft = -1;
    int cutRight = -1;
    bool vertical = verticalTabs(d->shape);
    QStyleOptionTab cutTabLeft;
    QStyleOptionTab cutTabRight;
    selected = d->currentIndex;
    if (d->dragInProgress)
        selected = d->pressedIndex;
    const QRect scrollRect = d->normalizedScrollRect();

    for (int i = 0; i < d->tabList.count(); ++i)
         optTabBase.tabBarRect |= tabRect(i);

    optTabBase.selectedTabRect = tabRect(selected);

    if (d->drawBase)
        p.drawPrimitive(QStyle::PE_FrameTabBarBase, optTabBase);

    for (int i = 0; i < d->tabList.count(); ++i) {
        QStyleOptionTab tab;
        initStyleOption(&tab, i);
        if (d->paintWithOffsets && d->tabList[i].dragOffset != 0) {
            if (vertical) {
                tab.rect.moveTop(tab.rect.y() + d->tabList[i].dragOffset);
            } else {
                tab.rect.moveLeft(tab.rect.x() + d->tabList[i].dragOffset);
            }
        }
        if (!(tab.state & QStyle::State_Enabled)) {
            tab.palette.setCurrentColorGroup(QPalette::Disabled);
        }

        // If this tab is partially obscured, make a note of it so that we can pass the information
        // along when we draw the tear.
        QRect tabRect = d->tabList[i].rect;
        int tabStart = vertical ? tabRect.top() : tabRect.left();
        int tabEnd = vertical ? tabRect.bottom() : tabRect.right();
        if (tabStart < scrollRect.left() + d->scrollOffset) {
            cutLeft = i;
            cutTabLeft = tab;
        } else if (tabEnd > scrollRect.right() + d->scrollOffset) {
            cutRight = i;
            cutTabRight = tab;
        }

        // Don't bother drawing a tab if the entire tab is outside of the visible tab bar.
        if ((!vertical && (tab.rect.right() < 0 || tab.rect.left() > width()))
            || (vertical && (tab.rect.bottom() < 0 || tab.rect.top() > height())))
            continue;

        optTabBase.tabBarRect |= tab.rect;
        if (i == selected)
            continue;

        p.drawControl(QStyle::CE_TabBarTab, tab);
    }

    // Draw the selected tab last to get it "on top"
    if (selected >= 0) {
        QStyleOptionTab tab;
        initStyleOption(&tab, selected);
        if (d->paintWithOffsets && d->tabList[selected].dragOffset != 0) {
            if (vertical)
                tab.rect.moveTop(tab.rect.y() + d->tabList[selected].dragOffset);
            else
                tab.rect.moveLeft(tab.rect.x() + d->tabList[selected].dragOffset);
        }
        if (!d->dragInProgress)
            p.drawControl(QStyle::CE_TabBarTab, tab);
        else {
            int taboverlap = style()->pixelMetric(QStyle::PM_TabBarTabOverlap, 0, this);
            if (verticalTabs(d->shape))
                d->movingTab->setGeometry(tab.rect.adjusted(0, -taboverlap, 0, taboverlap));
            else
                d->movingTab->setGeometry(tab.rect.adjusted(-taboverlap, 0, taboverlap, 0));
        }
    }

    // Only draw the tear indicator if necessary. Most of the time we don't need too.
    if (d->leftB->isVisible() && cutLeft >= 0) {
        cutTabLeft.rect = rect();
        cutTabLeft.rect = style()->subElementRect(QStyle::SE_TabBarTearIndicatorLeft, &cutTabLeft, this);
        p.drawPrimitive(QStyle::PE_IndicatorTabTearLeft, cutTabLeft);
    }

    if (d->rightB->isVisible() && cutRight >= 0) {
        cutTabRight.rect = rect();
        cutTabRight.rect = style()->subElementRect(QStyle::SE_TabBarTearIndicatorRight, &cutTabRight, this);
        p.drawPrimitive(QStyle::PE_IndicatorTabTearRight, cutTabRight);
    }
}

/*
    Given that index at position from moved to position to where return where index goes.
 */
int QTabBarExPrivate::calculateNewPosition(int from, int to, int index) const
{
    if (index == from)
        return to;

    int start = qMin(from, to);
    int end = qMax(from, to);
    if (index >= start && index <= end)
        index += (from < to) ? -1 : 1;
    return index;
}

/*!
    Moves the item at index position \a from to index position \a to.
    \since 4.5

    \sa tabMoved(), tabLayoutChange()
 */
void QTabBarEx::moveTab(int from, int to)
{
    Q_D(QTabBarEx);
    if (from == to
        || !d->validIndex(from)
        || !d->validIndex(to))
        return;

    bool vertical = verticalTabs(d->shape);
    int oldPressedPosition = 0;
    if (d->pressedIndex != -1) {
        // Record the position of the pressed tab before reordering the tabs.
        oldPressedPosition = vertical ? d->tabList[d->pressedIndex].rect.y()
                             : d->tabList[d->pressedIndex].rect.x();
    }

    // Update the locations of the tabs first
    int start = qMin(from, to);
    int end = qMax(from, to);
    int width = vertical ? d->tabList[from].rect.height() : d->tabList[from].rect.width();
    if (from < to)
        width *= -1;
    bool rtl = isRightToLeft();
    for (int i = start; i <= end; ++i) {
        if (i == from)
            continue;
        if (vertical)
            d->tabList[i].rect.moveTop(d->tabList[i].rect.y() + width);
        else
            d->tabList[i].rect.moveLeft(d->tabList[i].rect.x() + width);
        int direction = -1;
        if (rtl && !vertical)
            direction *= -1;
        if (d->tabList[i].dragOffset != 0)
            d->tabList[i].dragOffset += (direction * width);
    }

    if (vertical) {
        if (from < to)
            d->tabList[from].rect.moveTop(d->tabList[to].rect.bottom() + 1);
        else
            d->tabList[from].rect.moveTop(d->tabList[to].rect.top() - width);
    } else {
        if (from < to)
            d->tabList[from].rect.moveLeft(d->tabList[to].rect.right() + 1);
        else
            d->tabList[from].rect.moveLeft(d->tabList[to].rect.left() - width);
    }

    // Move the actual data structures
    d->tabList.move(from, to);

    // update lastTab locations
    for (int i = 0; i < d->tabList.count(); ++i)
        d->tabList[i].lastTab = d->calculateNewPosition(from, to, d->tabList[i].lastTab);

    // update external variables
    int previousIndex = d->currentIndex;
    d->currentIndex = d->calculateNewPosition(from, to, d->currentIndex);

    // If we are in the middle of a drag update the dragStartPosition
    if (d->pressedIndex != -1) {
        d->pressedIndex = d->calculateNewPosition(from, to, d->pressedIndex);
        int newPressedPosition = vertical ? d->tabList[d->pressedIndex].rect.top() : d->tabList[d->pressedIndex].rect.left();
        int diff = oldPressedPosition - newPressedPosition;
        if (isRightToLeft() && !vertical)
            diff *= -1;
        if (vertical)
            d->dragStartPosition.setY(d->dragStartPosition.y() - diff);
        else
            d->dragStartPosition.setX(d->dragStartPosition.x() - diff);
    }

    d->layoutWidgets(start);
    update();
    emit tabMoved(from, to);
    if (previousIndex != d->currentIndex)
        emit currentChanged(d->currentIndex);
    emit tabLayoutChange();
}

void QTabBarExPrivate::slide(int from, int to)
{
    Q_Q(QTabBarEx);
    if (from == to
            || !validIndex(from)
            || !validIndex(to))
        return;
    bool vertical = verticalTabs(shape);
    int preLocation = vertical ? q->tabRect(from).y() : q->tabRect(from).x();
    q->setUpdatesEnabled(false);
    q->moveTab(from, to);
    q->setUpdatesEnabled(true);
    int postLocation = vertical ? q->tabRect(to).y() : q->tabRect(to).x();
    int length = postLocation - preLocation;
    tabList[to].dragOffset -= length;
    tabList[to].startAnimation(this, ANIMATION_DURATION);
}

void QTabBarExPrivate::moveTab(int index, int offset)
{
    if (!validIndex(index))
        return;
    tabList[index].dragOffset = offset;
    layoutTab(index); // Make buttons follow tab
    q_func()->update();
}

/*!\reimp
*/
void QTabBarEx::mousePressEvent(QMouseEvent *event)
{
    Q_D(QTabBarEx);

    const QPoint pos = event->pos();
    const bool isEventInCornerButtons = (!d->leftB->isHidden() && d->leftB->geometry().contains(pos))
                                     || (!d->rightB->isHidden() && d->rightB->geometry().contains(pos));
    if (!isEventInCornerButtons) {
        const int index = d->indexAtPos(pos);
        emit tabBarClicked(index);
    }

    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    // Be safe!
    if (d->pressedIndex != -1 && d->movable)
        d->moveTabFinished(d->pressedIndex);

    d->pressedIndex = d->indexAtPos(event->pos());
#if 0 // Used to be included in Qt4 for Q_WS_MAC
    d->previousPressedIndex = d->pressedIndex;
#endif
    if (d->validIndex(d->pressedIndex)) {
        QStyleOptionTabBarBase optTabBase;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        optTabBase.init(this);
#else
        optTabBase.initFrom(this);
#endif

        optTabBase.documentMode = d->documentMode;
        if (event->type() == style()->styleHint(QStyle::SH_TabBar_SelectMouseType, &optTabBase, this))
            setCurrentIndex(d->pressedIndex);
        else
            repaint(tabRect(d->pressedIndex));
        if (d->movable) {
            d->dragStartPosition = event->pos();
        }
    }
}

/*!\reimp
 */
void QTabBarEx::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QTabBarEx);
    if (d->movable) {
        // Be safe!
        if (d->pressedIndex != -1
            && event->buttons() == Qt::NoButton)
            d->moveTabFinished(d->pressedIndex);

        // Start drag
        if (!d->dragInProgress && d->pressedIndex != -1) {
            if ((event->pos() - d->dragStartPosition).manhattanLength() > QApplication::startDragDistance()) {
                d->dragInProgress = true;
                d->setupMovableTab();
            }
        }

        if (event->buttons() == Qt::LeftButton
            && d->dragInProgress
            && d->validIndex(d->pressedIndex)) {
            bool vertical = verticalTabs(d->shape);
            int dragDistance;
            if (vertical) {
                dragDistance = (event->pos().y() - d->dragStartPosition.y());
            } else {
                dragDistance = (event->pos().x() - d->dragStartPosition.x());
            }
            d->tabList[d->pressedIndex].dragOffset = dragDistance;

            QRect startingRect = tabRect(d->pressedIndex);
            if (vertical)
                startingRect.moveTop(startingRect.y() + dragDistance);
            else
                startingRect.moveLeft(startingRect.x() + dragDistance);

            int overIndex;
            if (dragDistance < 0)
                overIndex = tabAt(startingRect.topLeft());
            else
                overIndex = tabAt(startingRect.topRight());

            if (overIndex != d->pressedIndex && overIndex != -1) {
                int offset = 1;
                if (isRightToLeft() && !vertical)
                    offset *= -1;
                if (dragDistance < 0) {
                    dragDistance *= -1;
                    offset *= -1;
                }
                for (int i = d->pressedIndex;
                     offset > 0 ? i < overIndex : i > overIndex;
                     i += offset) {
                    QRect overIndexRect = tabRect(overIndex);
                    int needsToBeOver = (vertical ? overIndexRect.height() : overIndexRect.width()) / 2;
                    if (dragDistance > needsToBeOver)
                        d->slide(i + offset, d->pressedIndex);
                }
            }
            // Buttons needs to follow the dragged tab
            d->layoutTab(d->pressedIndex);

            update();
        }
#if 0 // Used to be included in Qt4 for Q_WS_MAC
    } else if (!d->documentMode && event->buttons() == Qt::LeftButton && d->previousPressedIndex != -1) {
        int newPressedIndex = d->indexAtPos(event->pos());
        if (d->pressedIndex == -1 && d->previousPressedIndex == newPressedIndex) {
            d->pressedIndex = d->previousPressedIndex;
            update(tabRect(d->pressedIndex));
        } else if(d->pressedIndex != newPressedIndex) {
            d->pressedIndex = -1;
            update(tabRect(d->previousPressedIndex));
        }
#endif
    }

    if (event->buttons() != Qt::LeftButton) {
        event->ignore();
        return;
    }
}

void QTabBarExPrivate::setupMovableTab()
{
    Q_Q(QTabBarEx);
    if (!movingTab)
        movingTab = new QMovableTabWidget(q);

    int taboverlap = q->style()->pixelMetric(QStyle::PM_TabBarTabOverlap, 0 ,q);
    QRect grabRect = q->tabRect(pressedIndex);
    if (verticalTabs(shape))
        grabRect.adjust(0, -taboverlap, 0, taboverlap);
    else
        grabRect.adjust(-taboverlap, 0, taboverlap, 0);

    QPixmap grabImage(grabRect.size() * q->devicePixelRatioF());
    grabImage.setDevicePixelRatio(q->devicePixelRatioF());
    grabImage.fill(Qt::transparent);
    QStylePainter p(&grabImage, q);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    p.initFrom(q);
#endif

    QStyleOptionTab tab;
    q->initStyleOption(&tab, pressedIndex);
    tab.position = QStyleOptionTab::OnlyOneTab;
    if (verticalTabs(shape))
        tab.rect.moveTopLeft(QPoint(0, taboverlap));
    else
        tab.rect.moveTopLeft(QPoint(taboverlap, 0));
    p.drawControl(QStyle::CE_TabBarTab, tab);
    p.end();

    movingTab->setPixmap(grabImage);
    movingTab->setGeometry(grabRect);
    movingTab->raise();

    // Re-arrange widget order to avoid overlaps
    if (tabList[pressedIndex].leftWidget)
        tabList[pressedIndex].leftWidget->raise();
    if (tabList[pressedIndex].rightWidget)
        tabList[pressedIndex].rightWidget->raise();
    if (leftB)
        leftB->raise();
    if (rightB)
        rightB->raise();
    movingTab->setVisible(true);
}

void QTabBarExPrivate::moveTabFinished(int index)
{
    Q_Q(QTabBarEx);
    bool cleanup = (pressedIndex == index) || (pressedIndex == -1) || !validIndex(index);
    bool allAnimationsFinished = true;
#ifndef QT_NO_ANIMATION
    for(int i = 0; allAnimationsFinished && i < tabList.count(); ++i) {
        const Tab &t = tabList.at(i);
        if (t.animation && t.animation->state() == QAbstractAnimation::Running)
            allAnimationsFinished = false;
    }
#endif //QT_NO_ANIMATION
    if (allAnimationsFinished && cleanup) {
        if(movingTab)
            movingTab->setVisible(false); // We might not get a mouse release
        for (int i = 0; i < tabList.count(); ++i) {
            tabList[i].dragOffset = 0;
        }
        if (pressedIndex != -1 && movable) {
            pressedIndex = -1;
            dragInProgress = false;
            dragStartPosition = QPoint();
        }
        layoutWidgets();
    } else {
        if (!validIndex(index))
            return;
        tabList[index].dragOffset = 0;
    }
    q->update();
}

/*!\reimp
*/
void QTabBarEx::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QTabBarEx);
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
#if 0 // Used to be included in Qt4 for Q_WS_MAC
    d->previousPressedIndex = -1;
#endif
    if (d->movable && d->dragInProgress && d->validIndex(d->pressedIndex)) {
        int length = d->tabList[d->pressedIndex].dragOffset;
        int width = verticalTabs(d->shape)
            ? tabRect(d->pressedIndex).height()
            : tabRect(d->pressedIndex).width();
        int duration = qMin(ANIMATION_DURATION,
                (qAbs(length) * ANIMATION_DURATION) / width);
        d->tabList[d->pressedIndex].startAnimation(d, duration);
        d->dragInProgress = false;
        d->movingTab->setVisible(false);
        d->dragStartPosition = QPoint();
    }

    int i = d->indexAtPos(event->pos()) == d->pressedIndex ? d->pressedIndex : -1;
    d->pressedIndex = -1;
    QStyleOptionTabBarBase optTabBase;
    optTabBase.initFrom(this);
    optTabBase.documentMode = d->documentMode;
    const bool selectOnRelease =
            (style()->styleHint(QStyle::SH_TabBar_SelectMouseType, &optTabBase, this) == QEvent::MouseButtonRelease);
    if (selectOnRelease)
        setCurrentIndex(i);
    if (!selectOnRelease || !d->validIndex(i) || d->currentIndex == i)
        repaint(tabRect(i));
}

/*!\reimp
 */
void QTabBarEx::keyPressEvent(QKeyEvent *event)
{
    Q_D(QTabBarEx);
    if (event->key() != Qt::Key_Left && event->key() != Qt::Key_Right) {
        event->ignore();
        return;
    }
    int offset = event->key() == (isRightToLeft() ? Qt::Key_Right : Qt::Key_Left) ? -1 : 1;
    d->setCurrentNextEnabledIndex(offset);
}

/*!\reimp
 */
#if QT_CONFIG(wheelevent)
void QTabBarEx::wheelEvent(QWheelEvent *event)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#ifndef Q_OS_MAC
    Q_D(QTabBarEx);
    int offset = event->delta() > 0 ? -1 : 1;
    d->setCurrentNextEnabledIndex(offset);
    QWidget::wheelEvent(event);
#else
    Q_UNUSED(event)
#endif
#else
    // todo
#endif
}
#endif // QT_CONFIG(wheelevent)

void QTabBarExPrivate::setCurrentNextEnabledIndex(int offset)
{
    Q_Q(QTabBarEx);
    for (int index = currentIndex + offset; validIndex(index); index += offset) {
        if (tabList.at(index).enabled) {
            q->setCurrentIndex(index);
            break;
        }
    }
}

/*!\reimp
 */
void QTabBarEx::changeEvent(QEvent *event)
{
    Q_D(QTabBarEx);
    switch (event->type()) {
    case QEvent::StyleChange:
        if (!d->elideModeSetByUser)
            d->elideMode = Qt::TextElideMode(style()->styleHint(QStyle::SH_TabBar_ElideMode, 0, this));
        if (!d->useScrollButtonsSetByUser)
            d->useScrollButtons = !style()->styleHint(QStyle::SH_TabBar_PreferNoArrows, 0, this);
        Q_FALLTHROUGH();
    case QEvent::FontChange:
        d->textSizes.clear();
        d->refresh();
        break;
    default:
        break;
    }

    QWidget::changeEvent(event);
}

/*!
    \reimp
*/
void QTabBarEx::timerEvent(QTimerEvent *event)
{
    Q_D(QTabBarEx);
    if (event->timerId() == d->switchTabTimerId) {
        killTimer(d->switchTabTimerId);
        d->switchTabTimerId = 0;
        setCurrentIndex(d->switchTabCurrentIndex);
        d->switchTabCurrentIndex = -1;
    }
    QWidget::timerEvent(event);
}

/*!
    \property QTabBarEx::elideMode
    \brief how to elide text in the tab bar
    \since 4.2

    This property controls how items are elided when there is not
    enough space to show them for a given tab bar size.

    By default the value is style dependent.

    \sa QTabWidget::elideMode, usesScrollButtons, QStyle::SH_TabBar_ElideMode
*/

Qt::TextElideMode QTabBarEx::elideMode() const
{
    Q_D(const QTabBarEx);
    return d->elideMode;
}

void QTabBarEx::setElideMode(Qt::TextElideMode mode)
{
    Q_D(QTabBarEx);
    d->elideMode = mode;
    d->elideModeSetByUser = true;
    d->textSizes.clear();
    d->refresh();
}

/*!
    \property QTabBarEx::usesScrollButtons
    \brief Whether or not a tab bar should use buttons to scroll tabs when it
    has many tabs.
    \since 4.2

    When there are too many tabs in a tab bar for its size, the tab bar can either choose
    to expand its size or to add buttons that allow you to scroll through the tabs.

    By default the value is style dependant.

    \sa elideMode, QTabWidget::usesScrollButtons, QStyle::SH_TabBar_PreferNoArrows
*/
bool QTabBarEx::usesScrollButtons() const
{
    return d_func()->useScrollButtons;
}

void QTabBarEx::setUsesScrollButtons(bool useButtons)
{
    Q_D(QTabBarEx);
    d->useScrollButtonsSetByUser = true;
    if (d->useScrollButtons == useButtons)
        return;
    d->useScrollButtons = useButtons;
    d->refresh();
}

/*!
    \property QTabBarEx::tabsClosable
    \brief Whether or not a tab bar should place close buttons on each tab
    \since 4.5

    When tabsClosable is set to true a close button will appear on the tab on
    either the left or right hand side depending upon the style.  When the button
    is clicked the tab the signal tabCloseRequested will be emitted.

    By default the value is false.

    \sa setTabButton(), tabRemoved()
*/

bool QTabBarEx::tabsClosable() const
{
    Q_D(const QTabBarEx);
    return d->closeButtonOnTabs;
}

void QTabBarEx::setTabsClosable(bool closable)
{
    Q_D(QTabBarEx);
    if (d->closeButtonOnTabs == closable)
        return;
    d->closeButtonOnTabs = closable;
    ButtonPosition closeSide = (ButtonPosition)style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, 0, this);
    if (!closable) {
        for (int i = 0; i < d->tabList.count(); ++i) {
            if (closeSide == LeftSide && d->tabList[i].leftWidget) {
                d->tabList[i].leftWidget->deleteLater();
                d->tabList[i].leftWidget = 0;
            }
            if (closeSide == RightSide && d->tabList[i].rightWidget) {
                d->tabList[i].rightWidget->deleteLater();
                d->tabList[i].rightWidget = 0;
            }
        }
    } else {
        bool newButtons = false;
        for (int i = 0; i < d->tabList.count(); ++i) {
            if (tabButton(i, closeSide))
                continue;
            newButtons = true;
            QAbstractButton *closeButton = new CloseButton(this);
            connect(closeButton, SIGNAL(clicked()), this, SLOT(_q_closeTab()));
            setTabButton(i, closeSide, closeButton);
        }
        if (newButtons)
            d->layoutTabs();
    }
    update();
}

/*!
    \enum QTabBarEx::ButtonPosition
    \since 4.5

    This enum type lists the location of the widget on a tab.

    \value LeftSide Left side of the tab.

    \value RightSide Right side of the tab.

*/

/*!
    \enum QTabBarEx::SelectionBehavior
    \since 4.5

    This enum type lists the behavior of QTabBarEx when a tab is removed
    and the tab being removed is also the current tab.

    \value SelectLeftTab  Select the tab to the left of the one being removed.

    \value SelectRightTab  Select the tab to the right of the one being removed.

    \value SelectPreviousTab  Select the previously selected tab.

*/

/*!
    \property QTabBarEx::selectionBehaviorOnRemove
    \brief What tab should be set as current when removeTab is called if
    the removed tab is also the current tab.
    \since 4.5

    By default the value is SelectRightTab.

    \sa removeTab()
*/


QTabBarEx::SelectionBehavior QTabBarEx::selectionBehaviorOnRemove() const
{
    Q_D(const QTabBarEx);
    return d->selectionBehaviorOnRemove;
}

void QTabBarEx::setSelectionBehaviorOnRemove(QTabBarEx::SelectionBehavior behavior)
{
    Q_D(QTabBarEx);
    d->selectionBehaviorOnRemove = behavior;
}

/*!
    \property QTabBarEx::expanding
    \brief When expanding is true QTabBarEx will expand the tabs to use the empty space.
    \since 4.5

    By default the value is true.

    \sa QTabWidget::documentMode
*/

bool QTabBarEx::expanding() const
{
    Q_D(const QTabBarEx);
    return d->expanding;
}

void QTabBarEx::setExpanding(bool enabled)
{
    Q_D(QTabBarEx);
    if (d->expanding == enabled)
        return;
    d->expanding = enabled;
    d->layoutTabs();
}

/*!
    \property QTabBarEx::movable
    \brief This property holds whether the user can move the tabs
    within the tabbar area.

    \since 4.5

    By default, this property is \c false;
*/

bool QTabBarEx::isMovable() const
{
    Q_D(const QTabBarEx);
    return d->movable;
}

void QTabBarEx::setMovable(bool movable)
{
    Q_D(QTabBarEx);
    d->movable = movable;
}

bool QTabBarEx::isMultiRow() const
{
    Q_D(const QTabBarEx);
    return d->multiRow;
}

void QTabBarEx::setMultiRow(bool multiRow)
{
    Q_D(QTabBarEx);
    d->multiRow = multiRow;
}

/*!
    \property QTabBarEx::documentMode
    \brief Whether or not the tab bar is rendered in a mode suitable for the main window.
    \since 4.5

    This property is used as a hint for styles to draw the tabs in a different
    way then they would normally look in a tab widget.  On \macos this will
    look similar to the tabs in Safari or Sierra's Terminal.app.

    \sa QTabWidget::documentMode
*/
bool QTabBarEx::documentMode() const
{
    return d_func()->documentMode;
}

void QTabBarEx::setDocumentMode(bool enabled)
{
    Q_D(QTabBarEx);

    d->documentMode = enabled;
    d->updateMacBorderMetrics();
}

/*!
    \property QTabBarEx::autoHide
    \brief If true, the tab bar is automatically hidden when it contains less
    than 2 tabs.
    \since 5.4

    By default, this property is false.

    \sa QWidget::visible
*/

bool QTabBarEx::autoHide() const
{
    Q_D(const QTabBarEx);
    return d->autoHide;
}

void QTabBarEx::setAutoHide(bool hide)
{
    Q_D(QTabBarEx);
    if (d->autoHide == hide)
        return;

    d->autoHide = hide;
    if (hide)
        d->autoHideTabs();
    else
        setVisible(true);
}

/*!
    \property QTabBarEx::changeCurrentOnDrag
    \brief If true, then the current tab is automatically changed when dragging
    over the tabbar.
    \since 5.4

    \note You should also set acceptDrops property to true to make this feature
    work.

    By default, this property is false.
*/

bool QTabBarEx::changeCurrentOnDrag() const
{
    Q_D(const QTabBarEx);
    return d->changeCurrentOnDrag;
}

void QTabBarEx::setChangeCurrentOnDrag(bool change)
{
    Q_D(QTabBarEx);
    d->changeCurrentOnDrag = change;
    if (!change)
        d->killSwitchTabTimer();
}

/*!
    Sets \a widget on the tab \a index.  The widget is placed
    on the left or right hand side depending upon the \a position.
    \since 4.5

    Any previously set widget in \a position is hidden.

    The tab bar will take ownership of the widget and so all widgets set here
    will be deleted by the tab bar when it is destroyed unless you separately
    reparent the widget after setting some other widget (or 0).

    \sa tabsClosable()
  */
void QTabBarEx::setTabButton(int index, ButtonPosition position, QWidget *widget)
{
    Q_D(QTabBarEx);
    if (index < 0 || index >= d->tabList.count())
        return;
    if (widget) {
        widget->setParent(this);
        // make sure our left and right widgets stay on top
        widget->lower();
        widget->show();
    }
    if (position == LeftSide) {
        if (d->tabList[index].leftWidget)
            d->tabList[index].leftWidget->hide();
        d->tabList[index].leftWidget = widget;
    } else {
        if (d->tabList[index].rightWidget)
            d->tabList[index].rightWidget->hide();
        d->tabList[index].rightWidget = widget;
    }
    d->layoutTabs();
    d->refresh();
    update();
}

/*!
    Returns the widget set a tab \a index and \a position or 0 if
    one is not set.
  */
QWidget *QTabBarEx::tabButton(int index, ButtonPosition position) const
{
    Q_D(const QTabBarEx);
    if (index < 0 || index >= d->tabList.count())
        return 0;
    if (position == LeftSide)
        return d->tabList.at(index).leftWidget;
    else
        return d->tabList.at(index).rightWidget;
}

#ifndef QT_NO_ACCESSIBILITY
/*!
    Sets the accessibleName of the tab at position \a index to \a name.
*/
void QTabBarEx::setAccessibleTabName(int index, const QString &name)
{
    Q_D(QTabBarEx);
    if (QTabBarExPrivate::Tab *tab = d->at(index)) {
        tab->accessibleName = name;
        QAccessibleEvent event(this, QAccessible::NameChanged);
        event.setChild(index);
        QAccessible::updateAccessibility(&event);
    }
}

/*!
    Returns the accessibleName of the tab at position \a index, or an empty
    string if \a index is out of range.
*/
QString QTabBarEx::accessibleTabName(int index) const
{
    Q_D(const QTabBarEx);
    if (const QTabBarExPrivate::Tab *tab = d->at(index))
        return tab->accessibleName;
    return QString();
}
#endif // QT_NO_ACCESSIBILITY

CloseButton::CloseButton(QWidget *parent)
    : QAbstractButton(parent)
{
    setFocusPolicy(Qt::NoFocus);
#ifndef QT_NO_CURSOR
    setCursor(Qt::ArrowCursor);
#endif
#ifndef QT_NO_TOOLTIP
    setToolTip(tr("Close Tab"));
#endif
    resize(sizeHint());
}

QSize CloseButton::sizeHint() const
{
    ensurePolished();
    int width = style()->pixelMetric(QStyle::PM_TabCloseIndicatorWidth, 0, this);
    int height = style()->pixelMetric(QStyle::PM_TabCloseIndicatorHeight, 0, this);
    return QSize(width, height);
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void CloseButton::enterEvent(QEvent *event)
#else
void CloseButton::enterEvent(QEnterEvent *event)
#endif
{
    if (isEnabled())
        update();
    QAbstractButton::enterEvent(event);
}

void CloseButton::leaveEvent(QEvent *event)
{
    if (isEnabled())
        update();
    QAbstractButton::leaveEvent(event);
}

void CloseButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QStyleOption opt;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    opt.init(this);
#else
    opt.initFrom(this);
#endif
    opt.state |= QStyle::State_AutoRaise;
    if (isEnabled() && underMouse() && !isChecked() && !isDown())
        opt.state |= QStyle::State_Raised;
    if (isChecked())
        opt.state |= QStyle::State_On;
    if (isDown())
        opt.state |= QStyle::State_Sunken;

    if (const QTabBarEx *tb = qobject_cast<const QTabBarEx *>(parent())) {
        int index = tb->currentIndex();
        QTabBarEx::ButtonPosition position = (QTabBarEx::ButtonPosition)style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, 0, tb);
        if (tb->tabButton(index, position) == this)
            opt.state |= QStyle::State_Selected;
    }

    style()->drawPrimitive(QStyle::PE_IndicatorTabClose, &opt, &p, this);
}

#ifndef QT_NO_ANIMATION
void QTabBarExPrivate::Tab::TabBarAnimation::updateCurrentValue(const QVariant &current)
{
    priv->moveTab(priv->tabList.indexOf(*tab), current.toInt());
}

void QTabBarExPrivate::Tab::TabBarAnimation::updateState(QAbstractAnimation::State, QAbstractAnimation::State newState)
{
    if (newState == Stopped) priv->moveTabFinished(priv->tabList.indexOf(*tab));
}
#endif

QT_END_NAMESPACE
