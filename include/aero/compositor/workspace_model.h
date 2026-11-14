#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace Aero::Compositor {

/// Virtual desktop model.  WindowManager owns one instance; the shell and the
/// workspace overview observe it.
class WorkspaceModel : public QObject {
    Q_OBJECT
public:
    explicit WorkspaceModel(int count = 4, QObject* parent = nullptr);

    int count() const;
    int activeIndex() const;
    bool isValidIndex(int index) const;

    QString name(int index) const;
    QStringList names() const;
    void setName(int index, const QString& name);

    void setCount(int count);
    bool setActiveIndex(int index);

signals:
    void countChanged(int count);
    void activeIndexChanged(int index);
    void namesChanged();
    void workspaceAdded(int index);
    void workspaceRemoved(int index);

private:
    static QString defaultName(int index);

    QStringList m_names;
    int m_activeIndex = 0;
};

} // namespace Aero::Compositor
