#ifndef TCOMBOBOXDELEGATE_H
#define TCOMBOBOXDELEGATE_H

#include <QStyledItemDelegate>
#include <QStringList>

class TComboBoxDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    TComboBoxDelegate(QObject *parent = nullptr);
    void setItems(QStringList items, bool editable);


    // 自定义代理必须重新实现以下4个函数
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void setEditorData(QWidget *editor, const QModelIndex& index) const;
    void setModelData(QWidget* editor, QAbstractItemModel *model, const QModelIndex &index) const;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex& index) const;

private:
    QStringList m_itemList;        // 选项列表
    bool m_editable;               // 是否可编辑
};

#endif // TCOMBOBOXDELEGATE_H
