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

#include "qtabwidgetex_p.h"

#include "private/qwidget_p.h"
#include "qtabbarex_p.h"
#include "qapplication.h"
#include "qbitmap.h"
#include "qdesktopwidget.h"
#include <private/qdesktopwidget_p.h>
#include "qevent.h"
#include "qlayout.h"
#include "qstackedwidget.h"
#include "qstyle.h"
#include "qstyleoption.h"
#include "qstylepainter.h"
#include "qtabbarex.h"
#include "qtoolbutton.h"

QT_BEGIN_NAMESPACE

/*!
    \class QTabWidgetEx
    \brief The QTabWidgetEx class provides a stack of tabbed widgets.

    \ingroup organizers
    \ingroup basicwidgets
    \inmodule QtWidgets

    \image windows-tabwidget.png

    A tab widget provides a tab bar (see QTabBarEx) and a "page area"
    that is used to display pages related to each tab. By default, the
    tab bar is shown above the page area, but different configurations
    are available (see \l{TabPosition}). Each tab is associated with a
    different widget (called a page). Only the current page is shown in
    the page area; all the other pages are hidden. The user can show a
    different page by clicking on its tab or by pressing its
    Alt+\e{letter} shortcut if it has one.

    The normal way to use QTabWidgetEx is to do the following:
    \list 1
    \li Create a QTabWidgetEx.
    \li Create a QWidget for each of the pages in the tab dialog, but
       do not specify parent widgets for them.
    \li Insert child widgets into the page widget, using layouts to
       position them as normal.
    \li Call addTab() or insertTab() to put the page widgets into the
       tab widget, giving each tab a suitable label with an optional
       keyboard shortcut.
    \endlist

    The position of the tabs is defined by \l tabPosition, their shape
    by \l tabShape.

    The signal currentChanged() is emitted when the user selects a
    page.

    The current page index is available as currentIndex(), the current
    page widget with currentWidget().  You can retrieve a pointer to a
    page widget with a given index using widget(), and can find the
    index position of a widget with indexOf(). Use setCurrentWidget()
    or setCurrentIndex() to show a particular page.

    You can change a tab's text and icon using setTabText() or
    setTabIcon(). A tab and its associated page can be removed with
    removeTab().

    Each tab is either enabled or disabled at any given time (see
    setTabEnabled()). If a tab is enabled, the tab text is drawn
    normally and the user can select that tab. If it is disabled, the
    tab is drawn in a different way and the user cannot select that
    tab. Note that even if a tab is disabled, the page can still be
    visible, for example if all of the tabs happen to be disabled.

    Tab widgets can be a very good way to split up a complex dialog.
    An alternative is to use a QStackedWidget for which you provide some
    means of navigating between pages, for example, a QToolBar or a
    QListWidget.

    Most of the functionality in QTabWidgetEx is provided by a QTabBarEx
    (at the top, providing the tabs) and a QStackedWidget (most of the
    area, organizing the individual pages).

    \sa QTabBarEx, QStackedWidget, QToolBox, {Tab Dialog Example}
*/

/*!
    \enum QTabWidgetEx::TabPosition

    This enum type defines where QTabWidgetEx draws the tab row:

    \value North  The tabs are drawn above the pages.
    \value South  The tabs are drawn below the pages.
    \value West  The tabs are drawn to the left of the pages.
    \value East  The tabs are drawn to the right of the pages.
*/

/*!
    \enum QTabWidgetEx::TabShape

    This enum type defines the shape of the tabs:
    \value Rounded  The tabs are drawn with a rounded look. This is the default
                    shape.
    \value Triangular  The tabs are drawn with a triangular look.
*/

/*!
    \fn void QTabWidgetEx::currentChanged(int index)

    This signal is emitted whenever the current page index changes.
    The parameter is the new current page \a index position, or -1
    if there isn't a new one (for example, if there are no widgets
    in the QTabWidgetEx)

    \sa currentWidget(), currentIndex
*/

/*!
    \fn void QTabWidgetEx::tabCloseRequested(int index)
    \since 4.5

    This signal is emitted when the close button on a tab is clicked.
    The \a index is the index that should be removed.

    \sa setTabsClosable()
*/

/*!
    \fn void QTabWidgetEx::tabBarClicked(int index)

    This signal is emitted when user clicks on a tab at an \a index.

    \a index refers to the tab clicked, or -1 if no tab is under the cursor.

    \since 5.2
*/

/*!
    \fn void QTabWidgetEx::tabBarDoubleClicked(int index)

    This signal is emitted when the user double clicks on a tab at an \a index.

    \a index is the index of a clicked tab, or -1 if no tab is under the cursor.

    \since 5.2
*/

QTabWidgetExPrivate::QTabWidgetExPrivate()
    : tabs(0), stack(0), dirty(true),
      pos(QTabWidgetEx::North), shape(QTabWidgetEx::Rounded),
      leftCornerWidget(0), rightCornerWidget(0)
{}

QTabWidgetExPrivate::~QTabWidgetExPrivate()
{}

void QTabWidgetExPrivate::init()
{
    Q_Q(QTabWidgetEx);

    stack = new QStackedWidget(q);
    stack->setObjectName(QLatin1String("qt_tabwidget_stackedwidget"));
    stack->setLineWidth(0);
    // hack so that QMacStyle::layoutSpacing() can detect tab widget pages
    stack->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred, QSizePolicy::TabWidget));

    QObject::connect(stack, SIGNAL(widgetRemoved(int)), q, SLOT(_q_removeTab(int)));
    QTabBarEx *tabBar = new QTabBarEx(q);
    tabBar->setObjectName(QLatin1String("qt_tabwidget_tabbar"));
    tabBar->setDrawBase(false);
    q->setTabBar(tabBar);

    q->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding,
                                 QSizePolicy::TabWidget));
#ifdef QT_KEYPAD_NAVIGATION
    if (QApplication::keypadNavigationEnabled())
        q->setFocusPolicy(Qt::NoFocus);
    else
#endif
    q->setFocusPolicy(Qt::TabFocus);
    q->setFocusProxy(tabs);
    q->setTabPosition(static_cast<QTabWidgetEx::TabPosition> (q->style()->styleHint(
                      QStyle::SH_TabWidget_DefaultTabPosition, 0, q )));

}

/*!
    \reimp
*/

bool QTabWidgetEx::hasHeightForWidth() const
{
    Q_D(const QTabWidgetEx);
    bool has = d->size_policy.hasHeightForWidth();
    if (!has && d->stack)
        has = d->stack->hasHeightForWidth();
    return has;
}

/*!
    \internal

    Initialize only time inexpensive parts of the style option
    for QTabWidgetEx::setUpLayout()'s non-visible code path.
*/
void QTabWidgetExPrivate::initBasicStyleOption(QStyleOptionTabWidgetFrame *option) const
{
    Q_Q(const QTabWidgetEx);
    option->initFrom(q);

    if (q->documentMode())
        option->lineWidth = 0;
    else
        option->lineWidth = q->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, 0, q);

    switch (pos) {
    case QTabWidgetEx::North:
        option->shape = (QTabBar::Shape)(shape == QTabWidgetEx::Rounded ? QTabBarEx::RoundedNorth
                                                     : QTabBarEx::TriangularNorth);
        break;
    case QTabWidgetEx::South:
        option->shape = (QTabBar::Shape)(shape == QTabWidgetEx::Rounded ? QTabBarEx::RoundedSouth
                                                     : QTabBarEx::TriangularSouth);
        break;
    case QTabWidgetEx::West:
        option->shape = (QTabBar::Shape)(shape == QTabWidgetEx::Rounded ? QTabBarEx::RoundedWest
                                                     : QTabBarEx::TriangularWest);
        break;
    case QTabWidgetEx::East:
        option->shape = (QTabBar::Shape)(shape == QTabWidgetEx::Rounded ? QTabBarEx::RoundedEast
                                                     : QTabBarEx::TriangularEast);
        break;
    }

    option->tabBarRect = q->tabBar()->geometry();
}

/*!
    Initialize \a option with the values from this QTabWidgetEx. This method is useful
    for subclasses when they need a QStyleOptionTabWidgetFrame, but don't want to fill
    in all the information themselves.

    \sa QStyleOption::initFrom(), QTabBarEx::initStyleOption()
*/
void QTabWidgetEx::initStyleOption(QStyleOptionTabWidgetFrame *option) const
{
    if (!option)
        return;

    Q_D(const QTabWidgetEx);
    d->initBasicStyleOption(option);

    int exth = style()->pixelMetric(QStyle::PM_TabBarBaseHeight, 0, this);
    QSize t(0, d->stack->frameWidth());
    if (d->tabs->isVisibleTo(const_cast<QTabWidgetEx *>(this))) {
        t = d->tabs->sizeHint();
        if (documentMode()) {
            if (tabPosition() == East || tabPosition() == West) {
                t.setHeight(height());
            } else {
                t.setWidth(width());
            }
        }
    }

    if (d->rightCornerWidget) {
        const QSize rightCornerSizeHint = d->rightCornerWidget->sizeHint();
        const QSize bounds(rightCornerSizeHint.width(), t.height() - exth);
        option->rightCornerWidgetSize = rightCornerSizeHint.boundedTo(bounds);
    } else {
        option->rightCornerWidgetSize = QSize(0, 0);
    }

    if (d->leftCornerWidget) {
        const QSize leftCornerSizeHint = d->leftCornerWidget->sizeHint();
        const QSize bounds(leftCornerSizeHint.width(), t.height() - exth);
        option->leftCornerWidgetSize = leftCornerSizeHint.boundedTo(bounds);
    } else {
        option->leftCornerWidgetSize = QSize(0, 0);
    }

    option->tabBarSize = t;

    QRect selectedTabRect = tabBar()->tabRect(tabBar()->currentIndex());
    selectedTabRect.moveTopLeft(selectedTabRect.topLeft() + option->tabBarRect.topLeft());
    option->selectedTabRect = selectedTabRect;
}

/*!
    Constructs a tabbed widget with parent \a parent.
*/
QTabWidgetEx::QTabWidgetEx(QWidget *parent)
    : QWidget(*new QTabWidgetExPrivate, parent, 0)
{
    Q_D(QTabWidgetEx);
    d->init();
}


/*!
    Destroys the tabbed widget.
*/
QTabWidgetEx::~QTabWidgetEx()
{
}

/*!
    \fn int QTabWidgetEx::addTab(QWidget *page, const QString &label)

    Adds a tab with the given \a page and \a label to the tab widget,
    and returns the index of the tab in the tab bar. Ownership of \a page
    is passed on to the QTabWidgetEx.

    If the tab's \a label contains an ampersand, the letter following
    the ampersand is used as a shortcut for the tab, e.g. if the
    label is "Bro\&wse" then Alt+W becomes a shortcut which will
    move the focus to this tab.

    \note If you call addTab() after show(), the layout system will try
    to adjust to the changes in its widgets hierarchy and may cause
    flicker. To prevent this, you can set the QWidget::updatesEnabled
    property to false prior to changes; remember to set the property
    to true when the changes are done, making the widget receive paint
    events again.

    \sa insertTab()
*/
int QTabWidgetEx::addTab(QWidget *child, const QString &label)
{
    return insertTab(-1, child, label);
}


/*!
    \fn int QTabWidgetEx::addTab(QWidget *page, const QIcon &icon, const QString &label)
    \overload

    Adds a tab with the given \a page, \a icon, and \a label to the tab
    widget, and returns the index of the tab in the tab bar. Ownership
    of \a page is passed on to the QTabWidgetEx.

    This function is the same as addTab(), but with an additional \a
    icon.
*/
int QTabWidgetEx::addTab(QWidget *child, const QIcon& icon, const QString &label)
{
    return insertTab(-1, child, icon, label);
}


/*!
    \fn int QTabWidgetEx::insertTab(int index, QWidget *page, const QString &label)

    Inserts a tab with the given \a label and \a page into the tab
    widget at the specified \a index, and returns the index of the
    inserted tab in the tab bar. Ownership of \a page is passed on to the
    QTabWidgetEx.

    The label is displayed in the tab and may vary in appearance depending
    on the configuration of the tab widget.

    If the tab's \a label contains an ampersand, the letter following
    the ampersand is used as a shortcut for the tab, e.g. if the
    label is "Bro\&wse" then Alt+W becomes a shortcut which will
    move the focus to this tab.

    If \a index is out of range, the tab is simply appended.
    Otherwise it is inserted at the specified position.

    If the QTabWidgetEx was empty before this function is called, the
    new page becomes the current page. Inserting a new tab at an index
    less than or equal to the current index will increment the current
    index, but keep the current page.

    \note If you call insertTab() after show(), the layout system will try
    to adjust to the changes in its widgets hierarchy and may cause
    flicker. To prevent this, you can set the QWidget::updatesEnabled
    property to false prior to changes; remember to set the property
    to true when the changes are done, making the widget receive paint
    events again.

    \sa addTab()
*/
int QTabWidgetEx::insertTab(int index, QWidget *w, const QString &label)
{
    return insertTab(index, w, QIcon(), label);
}


/*!
    \fn int QTabWidgetEx::insertTab(int index, QWidget *page, const QIcon& icon, const QString &label)
    \overload

    Inserts a tab with the given \a label, \a page, and \a icon into
    the tab widget at the specified \a index, and returns the index of the
    inserted tab in the tab bar. Ownership of \a page is passed on to the
    QTabWidgetEx.

    This function is the same as insertTab(), but with an additional
    \a icon.
*/
int QTabWidgetEx::insertTab(int index, QWidget *w, const QIcon& icon, const QString &label)
{
    Q_D(QTabWidgetEx);
    if(!w)
        return -1;
    index = d->stack->insertWidget(index, w);
    d->tabs->insertTab(index, icon, label);
    setUpLayout();
    tabInserted(index);

    return index;
}


/*!
    Defines a new \a label for the page at position \a index's tab.

    If the provided text contains an ampersand character ('&'), a
    shortcut is automatically created for it. The character that
    follows the '&' will be used as the shortcut key. Any previous
    shortcut will be overwritten, or cleared if no shortcut is defined
    by the text. See the \l {QShortcut#mnemonic}{QShortcut}
    documentation for details (to display an actual ampersand, use
    '&&').

*/
void QTabWidgetEx::setTabText(int index, const QString &label)
{
    Q_D(QTabWidgetEx);
    d->tabs->setTabText(index, label);
    setUpLayout();
}

/*!
    Returns the label text for the tab on the page at position \a index.
*/

QString QTabWidgetEx::tabText(int index) const
{
    Q_D(const QTabWidgetEx);
    return d->tabs->tabText(index);
}

/*!
    \overload

    Sets the \a icon for the tab at position \a index.
*/
void QTabWidgetEx::setTabIcon(int index, const QIcon &icon)
{
    Q_D(QTabWidgetEx);
    d->tabs->setTabIcon(index, icon);
    setUpLayout();
}

/*!
    Returns the icon for the tab on the page at position \a index.
*/

QIcon QTabWidgetEx::tabIcon(int index) const
{
    Q_D(const QTabWidgetEx);
    return d->tabs->tabIcon(index);
}

/*!
    Returns \c true if the page at position \a index is enabled; otherwise returns \c false.

    \sa setTabEnabled(), QWidget::isEnabled()
*/

bool QTabWidgetEx::isTabEnabled(int index) const
{
    Q_D(const QTabWidgetEx);
    return d->tabs->isTabEnabled(index);
}

/*!
    If \a enable is true, the page at position \a index is enabled; otherwise the page at position \a index is
    disabled. The page's tab is redrawn appropriately.

    QTabWidgetEx uses QWidget::setEnabled() internally, rather than
    keeping a separate flag.

    Note that even a disabled tab/page may be visible. If the page is
    visible already, QTabWidgetEx will not hide it; if all the pages are
    disabled, QTabWidgetEx will show one of them.

    \sa isTabEnabled(), QWidget::setEnabled()
*/

void QTabWidgetEx::setTabEnabled(int index, bool enable)
{
    Q_D(QTabWidgetEx);
    d->tabs->setTabEnabled(index, enable);
    if (QWidget *widget = d->stack->widget(index))
        widget->setEnabled(enable);
}

/*!
  \fn void QTabWidgetEx::setCornerWidget(QWidget *widget, Qt::Corner corner)

  Sets the given \a widget to be shown in the specified \a corner of the
  tab widget. The geometry of the widget is determined based on the widget's
  sizeHint() and the style().

  Only the horizontal element of the \a corner will be used.

  Passing 0 shows no widget in the corner.

  Any previously set corner widget is hidden.

  All widgets set here will be deleted by the tab widget when it is
  destroyed unless you separately reparent the widget after setting
  some other corner widget (or 0).

  Note: Corner widgets are designed for \l North and \l South tab positions;
  other orientations are known to not work properly.

  \sa cornerWidget(), setTabPosition()
*/
void QTabWidgetEx::setCornerWidget(QWidget * widget, Qt::Corner corner)
{
    Q_D(QTabWidgetEx);
    if (widget && widget->parentWidget() != this)
        widget->setParent(this);

    if (corner & Qt::TopRightCorner) {
        if (d->rightCornerWidget)
            d->rightCornerWidget->hide();
        d->rightCornerWidget = widget;
    } else {
        if (d->leftCornerWidget)
            d->leftCornerWidget->hide();
        d->leftCornerWidget = widget;
    }
    setUpLayout();
}

/*!
    Returns the widget shown in the \a corner of the tab widget or 0.
*/
QWidget * QTabWidgetEx::cornerWidget(Qt::Corner corner) const
{
    Q_D(const QTabWidgetEx);
    if (corner & Qt::TopRightCorner)
        return d->rightCornerWidget;
    return d->leftCornerWidget;
}

/*!
   Removes the tab at position \a index from this stack of widgets.
   The page widget itself is not deleted.

   \sa addTab(), insertTab()
*/
void QTabWidgetEx::removeTab(int index)
{
    Q_D(QTabWidgetEx);
    if (QWidget *w = d->stack->widget(index))
        d->stack->removeWidget(w);
}

/*!
    Returns a pointer to the page currently being displayed by the tab
    dialog. The tab dialog does its best to make sure that this value
    is never 0 (but if you try hard enough, it can be).

    \sa currentIndex(), setCurrentWidget()
*/

QWidget * QTabWidgetEx::currentWidget() const
{
    Q_D(const QTabWidgetEx);
    return d->stack->currentWidget();
}

/*!
    Makes \a widget the current widget. The \a widget used must be a page in
    this tab widget.

    \sa addTab(), setCurrentIndex(), currentWidget()
 */
void QTabWidgetEx::setCurrentWidget(QWidget *widget)
{
    Q_D(const QTabWidgetEx);
    d->tabs->setCurrentIndex(indexOf(widget));
}


/*!
    \property QTabWidgetEx::currentIndex
    \brief the index position of the current tab page

    The current index is -1 if there is no current widget.

    By default, this property contains a value of -1 because there are initially
    no tabs in the widget.
*/

int QTabWidgetEx::currentIndex() const
{
    Q_D(const QTabWidgetEx);
    return d->tabs->currentIndex();
}

void QTabWidgetEx::setCurrentIndex(int index)
{
    Q_D(QTabWidgetEx);
    d->tabs->setCurrentIndex(index);
}


/*!
    Returns the index position of the page occupied by the widget \a
    w, or -1 if the widget cannot be found.
*/
int QTabWidgetEx::indexOf(QWidget* w) const
{
    Q_D(const QTabWidgetEx);
    return d->stack->indexOf(w);
}


/*!
    \reimp
*/
void QTabWidgetEx::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    setUpLayout();
}

/*!
    Replaces the dialog's QTabBarEx heading with the tab bar \a tb. Note
    that this must be called \e before any tabs have been added, or
    the behavior is undefined.

    \sa tabBar()
*/
void QTabWidgetEx::setTabBar(QTabBarEx* tb)
{
    Q_D(QTabWidgetEx);
    Q_ASSERT(tb);

    if (tb->parentWidget() != this) {
        tb->setParent(this);
        tb->show();
    }
    delete d->tabs;
    d->tabs = tb;
    setFocusProxy(d->tabs);
    connect(d->tabs, SIGNAL(currentChanged(int)),
            this, SLOT(_q_showTab(int)));
    connect(d->tabs, SIGNAL(tabMoved(int,int)),
            this, SLOT(_q_tabMoved(int,int)));
    connect(d->tabs, SIGNAL(tabBarClicked(int)),
            this, SIGNAL(tabBarClicked(int)));
    connect(d->tabs, SIGNAL(tabBarDoubleClicked(int)),
            this, SIGNAL(tabBarDoubleClicked(int)));
    if (d->tabs->tabsClosable())
        connect(d->tabs, SIGNAL(tabCloseRequested(int)),
                this, SIGNAL(tabCloseRequested(int)));
    tb->setExpanding(!documentMode());
    setUpLayout();
}


/*!
    Returns the current QTabBarEx.

    \sa setTabBar()
*/
QTabBarEx* QTabWidgetEx::tabBar() const
{
    Q_D(const QTabWidgetEx);
    return d->tabs;
}

/*
    Ensures that the selected tab's page is visible and appropriately
    sized.
*/

void QTabWidgetExPrivate::_q_showTab(int index)
{
    Q_Q(QTabWidgetEx);
    if (index < stack->count() && index >= 0)
        stack->setCurrentIndex(index);
    emit q->currentChanged(index);
}

void QTabWidgetExPrivate::_q_removeTab(int index)
{
    Q_Q(QTabWidgetEx);
    tabs->removeTab(index);
    q->setUpLayout();
    q->tabRemoved(index);
}

void QTabWidgetExPrivate::_q_tabMoved(int from, int to)
{
    const QSignalBlocker blocker(stack);
    QWidget *w = stack->widget(from);
    stack->removeWidget(w);
    stack->insertWidget(to, w);
}

/*
    Set up the layout.
    Get subrect from the current style, and set the geometry for the
    stack widget, tab bar and corner widgets.
*/
void QTabWidgetEx::setUpLayout(bool onlyCheck)
{
    Q_D(QTabWidgetEx);
    if (onlyCheck && !d->dirty)
        return; // nothing to do

    if (!isVisible()) {
        // this must be done immediately, because QWidgetItem relies on it (even if !isVisible())
        QStyleOptionTabWidgetFrame basicOption;
        d->initBasicStyleOption(&basicOption);
        d->setLayoutItemMargins(QStyle::SE_TabWidgetLayoutItem, &basicOption);
        d->dirty = true;
        return; // we'll do it later
    }

    QStyleOptionTabWidgetFrame option;
    initStyleOption(&option);
    d->setLayoutItemMargins(QStyle::SE_TabWidgetLayoutItem, &option);

    QRect tabRect = style()->subElementRect(QStyle::SE_TabWidgetTabBar, &option, this);
    d->panelRect = style()->subElementRect(QStyle::SE_TabWidgetTabPane, &option, this);
    QRect contentsRect = style()->subElementRect(QStyle::SE_TabWidgetTabContents, &option, this);
    QRect leftCornerRect = style()->subElementRect(QStyle::SE_TabWidgetLeftCorner, &option, this);
    QRect rightCornerRect = style()->subElementRect(QStyle::SE_TabWidgetRightCorner, &option, this);

	if(tabBar()->isMultiRow())
		tabRect.setWidth(contentsRect.width());
    d->tabs->setGeometry(tabRect);
    d->stack->setGeometry(contentsRect);
    if (d->leftCornerWidget)
        d->leftCornerWidget->setGeometry(leftCornerRect);
    if (d->rightCornerWidget)
        d->rightCornerWidget->setGeometry(rightCornerRect);

    if (!onlyCheck)
        update();
    updateGeometry();
}

/*!
    \internal
*/
static inline QSize basicSize(
    bool horizontal, const QSize &lc, const QSize &rc, const QSize &s, const QSize &t)
{
    return horizontal
        ? QSize(qMax(s.width(), t.width() + rc.width() + lc.width()),
                s.height() + (qMax(rc.height(), qMax(lc.height(), t.height()))))
        : QSize(s.width() + (qMax(rc.width(), qMax(lc.width(), t.width()))),
                qMax(s.height(), t.height() + rc.height() + lc.height()));
}

/*!
    \reimp
*/
QSize QTabWidgetEx::sizeHint() const
{
    Q_D(const QTabWidgetEx);
    QSize lc(0, 0), rc(0, 0);
    QStyleOptionTabWidgetFrame opt;
    initStyleOption(&opt);
    opt.state = QStyle::State_None;

    if (d->leftCornerWidget)
        lc = d->leftCornerWidget->sizeHint();
    if(d->rightCornerWidget)
        rc = d->rightCornerWidget->sizeHint();
    if (!d->dirty) {
        QTabWidgetEx *that = const_cast<QTabWidgetEx*>(this);
        that->setUpLayout(true);
    }
    QSize s(d->stack->sizeHint());
    QSize t;
    if (!d->isAutoHidden()) {
        t = d->tabs->sizeHint();
        if (usesScrollButtons())
            t = t.boundedTo(QSize(200,200));
        else
            t = t.boundedTo(QGuiApplication::primaryScreen()->virtualGeometry().size());
    }

    QSize sz = basicSize(d->pos == North || d->pos == South, lc, rc, s, t);

    return style()->sizeFromContents(QStyle::CT_TabWidget, &opt, sz, this)
                    .expandedTo(QApplication::globalStrut());
}


/*!
    \reimp

    Returns a suitable minimum size for the tab widget.
*/
QSize QTabWidgetEx::minimumSizeHint() const
{
    Q_D(const QTabWidgetEx);
    QSize lc(0, 0), rc(0, 0);

    if(d->leftCornerWidget)
        lc = d->leftCornerWidget->minimumSizeHint();
    if(d->rightCornerWidget)
        rc = d->rightCornerWidget->minimumSizeHint();
    if (!d->dirty) {
        QTabWidgetEx *that = const_cast<QTabWidgetEx*>(this);
        that->setUpLayout(true);
    }
    QSize s(d->stack->minimumSizeHint());
    QSize t;
    if (!d->isAutoHidden())
        t = d->tabs->minimumSizeHint();

    QSize sz = basicSize(d->pos == North || d->pos == South, lc, rc, s, t);

    QStyleOptionTabWidgetFrame opt;
    initStyleOption(&opt);
    opt.palette = palette();
    opt.state = QStyle::State_None;
    return style()->sizeFromContents(QStyle::CT_TabWidget, &opt, sz, this)
                    .expandedTo(QApplication::globalStrut());
}

/*!
    \reimp
*/
int QTabWidgetEx::heightForWidth(int width) const
{
    Q_D(const QTabWidgetEx);
    QStyleOptionTabWidgetFrame opt;
    initStyleOption(&opt);
    opt.state = QStyle::State_None;

    QSize zero(0,0);
    const QSize padding = style()->sizeFromContents(QStyle::CT_TabWidget, &opt, zero, this)
                                  .expandedTo(QApplication::globalStrut());

    QSize lc(0, 0), rc(0, 0);
    if (d->leftCornerWidget)
        lc = d->leftCornerWidget->sizeHint();
    if(d->rightCornerWidget)
        rc = d->rightCornerWidget->sizeHint();
    if (!d->dirty) {
        QTabWidgetEx *that = const_cast<QTabWidgetEx*>(this);
        that->setUpLayout(true);
    }
    QSize t;
    if (!d->isAutoHidden()) {
        t = d->tabs->sizeHint();
        if (usesScrollButtons())
            t = t.boundedTo(QSize(200,200));
        else
            t = t.boundedTo(QGuiApplication::primaryScreen()->virtualGeometry().size());
    }

    const bool tabIsHorizontal = (d->pos == North || d->pos == South);
    const int contentsWidth = width - padding.width();
    int stackWidth = contentsWidth;
    if (!tabIsHorizontal)
        stackWidth -= qMax(t.width(), qMax(lc.width(), rc.width()));

    int stackHeight = d->stack->heightForWidth(stackWidth);
    QSize s(stackWidth, stackHeight);

    QSize contentSize = basicSize(tabIsHorizontal, lc, rc, s, t);
    return (contentSize + padding).expandedTo(QApplication::globalStrut()).height();
}


/*!
    \reimp
 */
void QTabWidgetEx::showEvent(QShowEvent *)
{
    setUpLayout();
}

void QTabWidgetExPrivate::updateTabBarPosition()
{
    Q_Q(QTabWidgetEx);
    switch (pos) {
    case QTabWidgetEx::North:
        tabs->setShape(shape == QTabWidgetEx::Rounded ? QTabBarEx::RoundedNorth
                                                    : QTabBarEx::TriangularNorth);
        break;
    case QTabWidgetEx::South:
        tabs->setShape(shape == QTabWidgetEx::Rounded ? QTabBarEx::RoundedSouth
                                                    : QTabBarEx::TriangularSouth);
        break;
    case QTabWidgetEx::West:
        tabs->setShape(shape == QTabWidgetEx::Rounded ? QTabBarEx::RoundedWest
                                                    : QTabBarEx::TriangularWest);
        break;
    case QTabWidgetEx::East:
        tabs->setShape(shape == QTabWidgetEx::Rounded ? QTabBarEx::RoundedEast
                                                    : QTabBarEx::TriangularEast);
        break;
    }
    q->setUpLayout();
}

/*!
    \property QTabWidgetEx::tabPosition
    \brief the position of the tabs in this tab widget

    Possible values for this property are described by the TabPosition
    enum.

    By default, this property is set to \l North.

    \sa TabPosition
*/
QTabWidgetEx::TabPosition QTabWidgetEx::tabPosition() const
{
    Q_D(const QTabWidgetEx);
    return d->pos;
}

void QTabWidgetEx::setTabPosition(TabPosition pos)
{
    Q_D(QTabWidgetEx);
    if (d->pos == pos)
        return;
    d->pos = pos;
    d->updateTabBarPosition();
}

/*!
    \property QTabWidgetEx::tabsClosable
    \brief whether close buttons are automatically added to each tab.

    \since 4.5

    \sa QTabBarEx::tabsClosable()
*/
bool QTabWidgetEx::tabsClosable() const
{
    return tabBar()->tabsClosable();
}

void QTabWidgetEx::setTabsClosable(bool closeable)
{
    if (tabsClosable() == closeable)
        return;

    tabBar()->setTabsClosable(closeable);
    if (closeable)
        connect(tabBar(), SIGNAL(tabCloseRequested(int)),
                this, SIGNAL(tabCloseRequested(int)));
    else
        disconnect(tabBar(), SIGNAL(tabCloseRequested(int)),
                  this, SIGNAL(tabCloseRequested(int)));
    setUpLayout();
}

/*!
    \property QTabWidgetEx::movable
    \brief This property holds whether the user can move the tabs
    within the tabbar area.

    \since 4.5

    By default, this property is \c false;
*/

bool QTabWidgetEx::isMovable() const
{
    return tabBar()->isMovable();
}

void QTabWidgetEx::setMovable(bool movable)
{
    tabBar()->setMovable(movable);
}

bool QTabWidgetEx::isMultiRow() const
{
	return tabBar()->isMultiRow();
}

void QTabWidgetEx::setMultiRow(bool multiRow)
{
	tabBar()->setMultiRow(multiRow);
}

/*!
    \property QTabWidgetEx::tabShape
    \brief the shape of the tabs in this tab widget

    Possible values for this property are QTabWidgetEx::Rounded
    (default) or QTabWidgetEx::Triangular.

    \sa TabShape
*/

QTabWidgetEx::TabShape QTabWidgetEx::tabShape() const
{
    Q_D(const QTabWidgetEx);
    return d->shape;
}

void QTabWidgetEx::setTabShape(TabShape s)
{
    Q_D(QTabWidgetEx);
    if (d->shape == s)
        return;
    d->shape = s;
    d->updateTabBarPosition();
}

/*!
    \reimp
 */
bool QTabWidgetEx::event(QEvent *ev)
{
    if (ev->type() == QEvent::LayoutRequest)
        setUpLayout();
    return QWidget::event(ev);
}

/*!
    \reimp
 */
void QTabWidgetEx::changeEvent(QEvent *ev)
{
    if (ev->type() == QEvent::StyleChange
#ifdef Q_OS_MAC
            || ev->type() == QEvent::MacSizeChange
#endif
            )
        setUpLayout();
    QWidget::changeEvent(ev);
}


/*!
    \reimp
 */
void QTabWidgetEx::keyPressEvent(QKeyEvent *e)
{
    Q_D(QTabWidgetEx);
    if (((e->key() == Qt::Key_Tab || e->key() == Qt::Key_Backtab) &&
          count() > 1 && e->modifiers() & Qt::ControlModifier)
#ifdef QT_KEYPAD_NAVIGATION
          || QApplication::keypadNavigationEnabled() && (e->key() == Qt::Key_Left || e->key() == Qt::Key_Right) && count() > 1
#endif
       ) {
        int pageCount = d->tabs->count();
        int page = currentIndex();
        int dx = (e->key() == Qt::Key_Backtab || e->modifiers() & Qt::ShiftModifier) ? -1 : 1;
#ifdef QT_KEYPAD_NAVIGATION
        if (QApplication::keypadNavigationEnabled() && (e->key() == Qt::Key_Left || e->key() == Qt::Key_Right))
            dx = e->key() == (isRightToLeft() ? Qt::Key_Right : Qt::Key_Left) ? -1 : 1;
#endif
        for (int pass = 0; pass < pageCount; ++pass) {
            page+=dx;
            if (page < 0
#ifdef QT_KEYPAD_NAVIGATION
                && !e->isAutoRepeat()
#endif
               ) {
                page = count() - 1;
            } else if (page >= pageCount
#ifdef QT_KEYPAD_NAVIGATION
                       && !e->isAutoRepeat()
#endif
                      ) {
                page = 0;
            }
            if (d->tabs->isTabEnabled(page)) {
                setCurrentIndex(page);
                break;
            }
        }
        if (!QApplication::focusWidget())
            d->tabs->setFocus();
    } else {
        e->ignore();
    }
}

/*!
    Returns the tab page at index position \a index or 0 if the \a
    index is out of range.
*/
QWidget *QTabWidgetEx::widget(int index) const
{
    Q_D(const QTabWidgetEx);
    return d->stack->widget(index);
}

/*!
    \property QTabWidgetEx::count
    \brief the number of tabs in the tab bar

    By default, this property contains a value of 0.
*/
int QTabWidgetEx::count() const
{
    Q_D(const QTabWidgetEx);
    return d->tabs->count();
}

#ifndef QT_NO_TOOLTIP
/*!
    Sets the tab tool tip for the page at position \a index to \a tip.

    \sa tabToolTip()
*/
void QTabWidgetEx::setTabToolTip(int index, const QString & tip)
{
    Q_D(QTabWidgetEx);
    d->tabs->setTabToolTip(index, tip);
}

/*!
    Returns the tab tool tip for the page at position \a index or
    an empty string if no tool tip has been set.

    \sa setTabToolTip()
*/
QString QTabWidgetEx::tabToolTip(int index) const
{
    Q_D(const QTabWidgetEx);
    return d->tabs->tabToolTip(index);
}
#endif // QT_NO_TOOLTIP

#if QT_CONFIG(whatsthis)
/*!
    \since 4.1

    Sets the What's This help text for the page at position \a index
    to \a text.
*/
void QTabWidgetEx::setTabWhatsThis(int index, const QString &text)
{
    Q_D(QTabWidgetEx);
    d->tabs->setTabWhatsThis(index, text);
}

/*!
    \since 4.1

    Returns the What's This help text for the page at position \a index,
    or an empty string if no help text has been set.
*/
QString QTabWidgetEx::tabWhatsThis(int index) const
{
    Q_D(const QTabWidgetEx);
    return d->tabs->tabWhatsThis(index);
}
#endif // QT_CONFIG(whatsthis)

/*!
  This virtual handler is called after a new tab was added or
  inserted at position \a index.

  \sa tabRemoved()
 */
void QTabWidgetEx::tabInserted(int index)
{
    Q_UNUSED(index)
}

/*!
  This virtual handler is called after a tab was removed from
  position \a index.

  \sa tabInserted()
 */
void QTabWidgetEx::tabRemoved(int index)
{
    Q_UNUSED(index)
}

/*!
    \fn void QTabWidgetEx::paintEvent(QPaintEvent *event)

    Paints the tab widget's tab bar in response to the paint \a event.
*/
void QTabWidgetEx::paintEvent(QPaintEvent *)
{
    Q_D(QTabWidgetEx);
    if (documentMode()) {
        QStylePainter p(this, tabBar());
        if (QWidget *w = cornerWidget(Qt::TopLeftCorner)) {
            QStyleOptionTabBarBase opt;
            QTabBarExPrivate::initStyleBaseOption(&opt, tabBar(), w->size());
            opt.rect.moveLeft(w->x() + opt.rect.x());
            opt.rect.moveTop(w->y() + opt.rect.y());
            p.drawPrimitive(QStyle::PE_FrameTabBarBase, opt);
        }
        if (QWidget *w = cornerWidget(Qt::TopRightCorner)) {
            QStyleOptionTabBarBase opt;
            QTabBarExPrivate::initStyleBaseOption(&opt, tabBar(), w->size());
            opt.rect.moveLeft(w->x() + opt.rect.x());
            opt.rect.moveTop(w->y() + opt.rect.y());
            p.drawPrimitive(QStyle::PE_FrameTabBarBase, opt);
        }
        return;
    }
    QStylePainter p(this);

    QStyleOptionTabWidgetFrame opt;
    initStyleOption(&opt);
    opt.rect = d->panelRect;
    p.drawPrimitive(QStyle::PE_FrameTabWidget, opt);
}

/*!
    \property QTabWidgetEx::iconSize
    \brief The size for icons in the tab bar
    \since 4.2

    The default value is style-dependent. This is the maximum size
    that the icons will have. Icons are not scaled up if they are of
    smaller size.

    \sa QTabBarEx::iconSize
*/
QSize QTabWidgetEx::iconSize() const
{
    return d_func()->tabs->iconSize();
}

void QTabWidgetEx::setIconSize(const QSize &size)
{
    d_func()->tabs->setIconSize(size);
}

/*!
    \property QTabWidgetEx::elideMode
    \brief how to elide text in the tab bar
    \since 4.2

    This property controls how items are elided when there is not
    enough space to show them for a given tab bar size.

    By default the value is style dependant.

    \sa QTabBarEx::elideMode, usesScrollButtons, QStyle::SH_TabBar_ElideMode
*/
Qt::TextElideMode QTabWidgetEx::elideMode() const
{
    return d_func()->tabs->elideMode();
}

void QTabWidgetEx::setElideMode(Qt::TextElideMode mode)
{
    d_func()->tabs->setElideMode(mode);
}

/*!
    \property QTabWidgetEx::usesScrollButtons
    \brief Whether or not a tab bar should use buttons to scroll tabs when it
    has many tabs.
    \since 4.2

    When there are too many tabs in a tab bar for its size, the tab bar can either choose
    to expand its size or to add buttons that allow you to scroll through the tabs.

    By default the value is style dependant.

    \sa elideMode, QTabBarEx::usesScrollButtons, QStyle::SH_TabBar_PreferNoArrows
*/
bool QTabWidgetEx::usesScrollButtons() const
{
    return d_func()->tabs->usesScrollButtons();
}

void QTabWidgetEx::setUsesScrollButtons(bool useButtons)
{
    d_func()->tabs->setUsesScrollButtons(useButtons);
}

/*!
    \property QTabWidgetEx::documentMode
    \brief Whether or not the tab widget is rendered in a mode suitable for document
     pages. This is the same as document mode on \macos.
    \since 4.5

    When this property is set the tab widget frame is not rendered. This mode is useful
    for showing document-type pages where the page covers most of the tab widget
    area.

    \sa elideMode, QTabBarEx::documentMode, QTabBarEx::usesScrollButtons, QStyle::SH_TabBar_PreferNoArrows
*/
bool QTabWidgetEx::documentMode() const
{
    Q_D(const QTabWidgetEx);
    return d->tabs->documentMode();
}

void QTabWidgetEx::setDocumentMode(bool enabled)
{
    Q_D(QTabWidgetEx);
    d->tabs->setDocumentMode(enabled);
    d->tabs->setExpanding(!enabled);
    d->tabs->setDrawBase(enabled);
    setUpLayout();
}

/*!
    \property QTabWidgetEx::tabBarAutoHide
    \brief If true, the tab bar is automatically hidden when it contains less
    than 2 tabs.
    \since 5.4

    By default, this property is false.

    \sa QWidget::visible
*/

bool QTabWidgetEx::tabBarAutoHide() const
{
    Q_D(const QTabWidgetEx);
    return d->tabs->autoHide();
}

void QTabWidgetEx::setTabBarAutoHide(bool enabled)
{
    Q_D(QTabWidgetEx);
    return d->tabs->setAutoHide(enabled);
}

/*!
    Removes all the pages, but does not delete them. Calling this function
    is equivalent to calling removeTab() until the tab widget is empty.
*/
void QTabWidgetEx::clear()
{
    // ### optimize by introduce QStackedLayout::clear()
    while (count())
        removeTab(0);
}

QT_END_NAMESPACE

