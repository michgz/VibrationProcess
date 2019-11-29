#include "tablewidget.h"
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QTableView>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QVXYModelMapper>
#include <QtWidgets/QHeaderView>

#include "loadtrace.h"

QT_CHARTS_USE_NAMESPACE

static void setTraceSeries(t_Trace t1, QLineSeries *l1, unsigned int n)
{
    size_t i;

    for (i = 0; i < t1.vals.size(); i ++)
    {
        l1->append((static_cast<double>(i))/t1.frequency, static_cast<double>(t1.vals[i][n]));
    }

}

void TableWidget::ShowTrace(QTreeWidgetItem *newItem)
{
    t_Trace *p_t = getTrace(newItem->data(0, Qt::UserRole+1).toInt());

    QChart * const theChart = chart();

    if (theChart == nullptr)
        return;

    theChart->removeAllSeries();

    QLineSeries *series = new QLineSeries;
    series->setName("X");
    setTraceSeries(*p_t, series, 0);
    theChart->addSeries(series);

    series = new QLineSeries;
    series->setName("Y");
    setTraceSeries(*p_t, series, 1);
    theChart->addSeries(series);

    series = new QLineSeries;
    series->setName("Z");
    setTraceSeries(*p_t, series, 2);
    theChart->addSeries(series);

    theChart->createDefaultAxes();
}
