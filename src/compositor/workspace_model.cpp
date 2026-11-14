#include "aero/compositor/workspace_model.h"

namespace Aero::Compositor {

WorkspaceModel::WorkspaceModel(int count, QObject* parent)
    : QObject(parent)
{
    setCount(count);
}

int WorkspaceModel::count() const
{
    return m_names.size();
}

int WorkspaceModel::activeIndex() const
{
    return m_activeIndex;
}

bool WorkspaceModel::isValidIndex(int index) const
{
    return index >= 0 && index < m_names.size();
}

QString WorkspaceModel::name(int index) const
{
    return isValidIndex(index) ? m_names.at(index) : QString();
}

QStringList WorkspaceModel::names() const
{
    return m_names;
}

void WorkspaceModel::setName(int index, const QString& name)
{
    if (!isValidIndex(index) || name.trimmed().isEmpty() || m_names.at(index) == name) {
        return;
    }
    m_names[index] = name.trimmed();
    emit namesChanged();
}

void WorkspaceModel::setCount(int count)
{
    count = qBound(1, count, 16);
    if (count == m_names.size()) {
        return;
    }

    while (m_names.size() > count) {
        const int removed = m_names.size() - 1;
        m_names.removeLast();
        emit workspaceRemoved(removed);
    }
    while (m_names.size() < count) {
        const int added = m_names.size();
        m_names.append(defaultName(added));
        emit workspaceAdded(added);
    }

    if (m_activeIndex >= m_names.size()) {
        m_activeIndex = m_names.size() - 1;
        emit activeIndexChanged(m_activeIndex);
    }
    emit countChanged(m_names.size());
}

bool WorkspaceModel::setActiveIndex(int index)
{
    if (!isValidIndex(index) || index == m_activeIndex) {
        return isValidIndex(index);
    }
    m_activeIndex = index;
    emit activeIndexChanged(index);
    return true;
}

QString WorkspaceModel::defaultName(int index)
{
    switch (index) {
    case 0: return QStringLiteral("Primary");
    case 1: return QStringLiteral("Work");
    case 2: return QStringLiteral("Creative");
    case 3: return QStringLiteral("Focus");
    default: return QStringLiteral("Workspace %1").arg(index + 1);
    }
}

} // namespace Aero::Compositor
