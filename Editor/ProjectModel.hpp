#ifndef AMUSE_PROJECT_MODEL_HPP
#define AMUSE_PROJECT_MODEL_HPP

#include <QAbstractItemModel>

class ProjectModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit ProjectModel(QObject* parent = Q_NULLPTR);
};


#endif //AMUSE_PROJECT_MODEL_HPP
