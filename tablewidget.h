#ifndef TABLEWIDGET_H
#define TABLEWIDGET_H

#include <QtWidgets/QWidget>
#include <QTreeWidgetItem>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QFileSystemModel>
#include <QFileDialog>

#include "loadtrace.h"

QT_CHARTS_USE_NAMESPACE

class MyModel : public QFileSystemModel
{
    Q_OBJECT

public:
    MyModel(QWidget *parent);
    QFileDialog *saveDialog;
    QFileDialog *openDialog;
    QTreeWidget * treeWidget;

    t_Traces * theTraces;
    const bool saveWithWindowedMax = true;
private:
    void set_x(unsigned int);
    void setTree(QTreeWidget * treeWidgetTopLevel);
    QDir  currentDirectory;
    bool  haveCurrentDirectory;

public slots:
    void set_1(void);
    void set_0(void);

    void save(void);
    void open(void);
};

class TableWidget : public QChartView
{
    Q_OBJECT

public slots:
    void ShowTrace(QTreeWidgetItem *newItem);

};

#endif // TABLEWIDGET_H
