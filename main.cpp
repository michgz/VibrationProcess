#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QFileDialog>

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QFileSystemModel>
#include <QChartView>
#include <QListView>
#include <QAction>
#include <QTextStream>
#include <QtMath>

#include "tablewidget.h"
#include "loadtrace.h"

#include <QSqlQuery>
#include <QSqlQueryModel>
#include "sql/connection.h"

// Declare two brushes that will be used for all tree rows
static QBrush excBrush = QBrush(QColor(0xDE, 0x85, 0x85));
static QBrush nonexcBrush = QBrush(Qt::white);


MyModel::MyModel(QWidget *parent)
{
    haveCurrentDirectory = false;

    QFileDialog * d2 = new QFileDialog(parent);
    d2->setAcceptMode(QFileDialog::AcceptSave);
    saveDialog = d2;

    QFileDialog * d1 = new QFileDialog(parent);
    d1->setAcceptMode(QFileDialog::AcceptOpen);
    openDialog = d1;

}

void MyModel::set_1(void)
{
    // Set the exclusion class to be "1"
    set_x(1);
}

void MyModel::set_0(void)
{
    // Set the exclusion class to be "0" (i.e. Not excluded)
    set_x(0);
}

void MyModel::set_x(unsigned int k)
{
    // If the current directory is not set, there's nothing we can do (other than
    // possibly set the row colour). Should not get this situation.
    if (!haveCurrentDirectory || !createConnection(currentDirectory))
        return;

    int m = treeWidget->currentItem()->data(0, Qt::UserRole+1).toInt();

    if (m < theTraces->size())
    {
        QSqlQuery query;
        query.prepare("select id, exclusion from trace where (filename=:filename and datetime=:datetime)");
        query.bindValue(":filename", theTraces->at(m).fileName);
        query.bindValue(":datetime", theTraces->at(m).dt.toSecsSinceEpoch());
        query.exec();
        if (query.first())
        {
            int id_1 = query.value(0).toInt();
            query.prepare("update trace set exclusion=:k where id=:id");
            query.bindValue(":k", k);
            query.bindValue(":id", id_1);
            query.exec();
            // Don't care about results
        }
        else
        {
            query.prepare("insert into trace (filename, datetime, exclusion) values (:filename, :datetime, :k)");
            query.bindValue(":filename", theTraces->at(m).fileName);
            query.bindValue(":datetime", theTraces->at(m).dt.toSecsSinceEpoch());
            query.bindValue(":k", k);
            query.exec();
        }

        // Now set up a non-const reference to write to.
        t_Trace &newT = (*theTraces)[m];
        newT.exclusion = k;
    }

    QTreeWidgetItem *leaf = treeWidget->currentItem();

    int j;
    for(j = 0; j < leaf->columnCount(); j ++)
    {
        if (k > 0)
        {
            leaf->setBackground(j, excBrush);
        }
        else
        {
            leaf->setBackground(j, nonexcBrush);
        }
    }

}

void MyModel::setTree(QTreeWidget * treeWidgetTopLevel)
{
    treeWidgetTopLevel->clear();

    for(int end = theTraces->size(), i = 0; i < end; i ++)
    {
        QTreeWidgetItem *leaf = new QTreeWidgetItem();
        leaf->setText(0, theTraces->at(i).fileName);
        leaf->setText(1, theTraces->at(i).dt.toString("dd-HH:mm:ss") );
        leaf->setText(2, QString::number(static_cast<qreal>(theTraces->at(i).maximumDeviation)));
        leaf->setText(3, QString::number(static_cast<qreal>(theTraces->at(i).rmsDeviation)));
        if (saveWithWindowedMax)
        {
            qreal x = static_cast<qreal>(theTraces->at(i).wMax);
            if (x > 0.)
            {
                //leaf->setText(4, QString::number(20.*qLn(x/16384./1.E-6)/qLn(10.), 'f', 1));  // units of dB ug (Sometimes required)
                leaf->setText(4, QString::number(x, 'f', 3));  // same units as max/rms above
            }
            else
            {
                leaf->setText(4, "");
            }
        }
        if (theTraces->at(i).exclusion > 0)
        {
            for(int j = 0; j < leaf->columnCount(); j ++)
            {
                leaf->setBackground(j, excBrush);
            }
        }
        else
        {
            for(int j = 0; j < leaf->columnCount(); j ++)
            {
                leaf->setBackground(j, nonexcBrush);
            }
        }
        leaf->setData(0, Qt::UserRole+1, QVariant(static_cast<uint>(i)));
        treeWidgetTopLevel->addTopLevelItem(leaf);
    }
}

void MyModel::open(void)
{
    openDialog->setDefaultSuffix("CSV");
    openDialog->setFileMode(QFileDialog::ExistingFiles);

    QStringList filters;
    filters << "CSV files (*.csv *.CSV)"
            << "Any files (*)";
    openDialog->setNameFilters(filters);
    openDialog->exec();

    if (openDialog->result() == QDialog::Accepted && openDialog->selectedFiles().size() >= 1)
    {
        currentDirectory = openDialog->directory();
        QStringList allFiles = openDialog->selectedFiles();
        allFiles.sort(Qt::CaseInsensitive);
        haveCurrentDirectory = true;

        QList<QFileInfo> q;
        for(int end=allFiles.size(), i = 0; i < end; i ++)
        {
            q.push_back(QFileInfo(currentDirectory, allFiles.at(i)));
        }
        theTraces = loadtrace(currentDirectory, q);
        processExclusions(currentDirectory);

        // Windowed maximums must be calculated on the full vector -- after it's been loaded and processed for exclusions.
        if (saveWithWindowedMax)
        {
            addWindowedMax();
        }

        setTree(treeWidget);
    }
}

void MyModel::save(void)
{
    if (!haveCurrentDirectory)
        return;

    saveDialog->setDirectory(currentDirectory);
    saveDialog->setDefaultSuffix("CSV");
    saveDialog->selectFile(currentDirectory.dirName() + QDateTime::currentDateTime().toString("_yyMMdd_HHmmss"));   // set a starting filename

    QStringList filters;
    filters << "CSV files (*.csv *.CSV)"
            << "Any files (*)";
    saveDialog->setNameFilters(filters);
    saveDialog->exec();

    if (saveDialog->result() == QDialog::Accepted && saveDialog->selectedFiles().size() >= 1)
    {
        QFile file;
        file.setFileName(saveDialog->selectedFiles().at(0));
        if(file.open(QFile::WriteOnly))
        {
            QTextStream out(&file);
            out << "File name,Date/time,Max., RMS,";
            if (saveWithWindowedMax)
            {
                out << "Windowed Max.,";
            }
            out << "Excluded?" << "\n";

            int i = 0;
            while (true)
            {
                t_Trace * p_t = getTrace(i);
                if (p_t == nullptr)
                {
                    break;
                }
                else
                {
                    out << p_t->fileName
                        << "," << p_t->dt.toString("dd/MM/yyyy HH:mm:ss")
                        << "," << QString::number(static_cast<qreal>(p_t->maximumDeviation))
                        << "," << QString::number(static_cast<qreal>(p_t->rmsDeviation));
                    if (saveWithWindowedMax)
                    {
                        out << "," << QString::number(static_cast<qreal>(p_t->wMax));
                    }
                    out << ",";
                    if (p_t->exclusion > 0)
                    {
                        out << "X";
                    }
                    // else nothing...
                    out << "\n";
                }
                i ++;
            }


            // Now calculate VDV values
            t_VDVs vs = postProcessVdv();
            out << "Start,End,VDV [m s^-1.75]" << "\n";
            for(int endj = vs.size(), j = 0; j < endj; j ++)
            {
                out << vs[j].start.toString("dd/MM/yyyy HH:mm:ss")
                    << "," << vs[j].end.toString("dd/MM/yyyy HH:mm:ss")
                    << "," << QString::number(static_cast<qreal>(vs[j].total_VDV)) << "\n";
            }

            // Now output heartbeat information
            out << "File name,Date/time,Type,V_bat [V],Temp 1 [degC],Temp2 [degC],Temp3 [degC]" << "\n";
            i = 0;
            while (true)
            {
                t_Extra * p_x = getExtra(i);
                if (p_x == nullptr)
                {
                    break;
                }
                else
                {
                    out << p_x->fileName
                        << "," << p_x->dt.toString("dd/MM/yyyy HH:mm:ss");
                    if (p_x->type == t_ExtraType::Heartbeat)
                    {
                        out << ",HEARTBEAT";
                    }
                    else if (p_x->type == t_ExtraType::On)
                    {
                        out << ",ON";
                    }
                    else
                    {
                        out << ",";
                    }

                    out << "," << QString::number(static_cast<qreal>(p_x->v_bat))
                        << "," << QString::number(static_cast<qreal>(p_x->temp_1))
                        << "," << QString::number(static_cast<qreal>(p_x->temp_2))
                        << ",";
                    if (p_x->temp_3 != -1.0)  // comparison with float -- not good!
                    {
                        out << "," << QString::number(static_cast<qreal>(p_x->temp_3));
                    }
                    out << "\n";
                }
                i ++;
            }
        }
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QWidget *window = new QWidget;
    QHBoxLayout *mainLayout = new QHBoxLayout;
    QVBoxLayout *listLayout = new QVBoxLayout;
    QHBoxLayout *buttonsLayout = new QHBoxLayout;

    QPushButton *b1 = new QPushButton(QPushButton::tr("&Open"));
    b1->resize(50, 250);
    buttonsLayout->addWidget(b1);
    QPushButton *b2 = new QPushButton(QPushButton::tr("&Save"));
    b1->resize(50, 250);
    buttonsLayout->addWidget(b2);

    listLayout->addLayout(buttonsLayout);

    MyModel *model = new MyModel(window);

    QTreeWidget *treeWidget = new QTreeWidget(window);

    treeWidget->setColumnCount(4);
    QStringList headers;
    headers << QTreeWidget::tr("File") << QTreeWidget::tr("Date/Time") << QTreeWidget::tr("Max") << QTreeWidget::tr("R.M.S.");
    if (model->saveWithWindowedMax)
    {
        headers << QTreeWidget::tr("Wind. Max.");
    }
    treeWidget->setHeaderLabels(headers);

    a.connect(b1, &QPushButton::clicked, model, &MyModel::open);
    a.connect(b2, &QPushButton::clicked, model, &MyModel::save);

    model->treeWidget = treeWidget;

    QAction *action_1 = new QAction(QApplication::tr("&1"), treeWidget);
    action_1->setShortcut(QKeySequence(Qt::Key_1));
    action_1->setStatusTip(QApplication::tr("Set exclusion class 1"));
    treeWidget->addAction(action_1);
    a.connect(action_1, &QAction::triggered, model, &MyModel::set_1);

    QAction *action_0 = new QAction(QApplication::tr("&0"), treeWidget);
    action_0->setShortcut(QKeySequence(Qt::Key_0));
    action_0->setStatusTip(QApplication::tr("Clear exclusion class"));
    treeWidget->addAction(action_0);
    a.connect(action_0, &QAction::triggered, model, &MyModel::set_0);

    treeWidget->setMinimumWidth(450);
    listLayout->addWidget(treeWidget);

    mainLayout->addItem(listLayout);

    TableWidget w;

    QChart *chart = new QChart;
    chart->setAnimationOptions(QChart::NoAnimation);

    w.setRenderHint(QPainter::Antialiasing);
    w.setMinimumSize(640, 480);
    w.setChart(chart);

    mainLayout->addWidget(&w);
    window->setLayout(mainLayout);

    a.connect(treeWidget, &QTreeWidget::currentItemChanged, &w, &TableWidget::ShowTrace);

    window->show();

    return a.exec();
}
