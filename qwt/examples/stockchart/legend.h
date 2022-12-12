/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QwtAbstractLegend>

class LegendTreeView;
class QStandardItem;
class QModelIndex;
class QwtPlotItem;

class Legend : public QwtAbstractLegend
{
    Q_OBJECT

  public:
    explicit Legend( QWidget* parent = NULL );
    virtual ~Legend();

    virtual void renderLegend( QPainter*,
        const QRectF&, bool fillBackground ) const QWT_OVERRIDE;

    virtual bool isEmpty() const QWT_OVERRIDE;

    virtual int scrollExtent( Qt::Orientation ) const QWT_OVERRIDE;

  Q_SIGNALS:
    void checked( QwtPlotItem* plotItem, bool on, int index );

  public Q_SLOTS:
    virtual void updateLegend( const QVariant&,
        const QList< QwtLegendData >& ) QWT_OVERRIDE;

  private Q_SLOTS:
    void handleClick( const QModelIndex& );

  private:
    void updateItem( QStandardItem*, const QwtLegendData& );

    LegendTreeView* m_treeView;
};
