#ifndef AMUSE_PROJECT_STATISTICS_HPP
#define AMUSE_PROJECT_STATISTICS_HPP

#include <QAbstractTableModel>

class ProjectStatistics : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit ProjectStatistics(QObject* parent = Q_NULLPTR);
};


#endif //AMUSE_PROJECT_STATISTICS_HPP
