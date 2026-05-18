#include "texture_list_widget.h"
#include <QFileInfo>
#include <QMessageBox>
#include <spdlog/spdlog.h>

TextureListWidget::TextureListWidget(QWidget *parent)
    : QTreeWidget(parent)
    , remove_path_action_(nullptr)
    , selected_patch_id_(-1)
{
    setHeaderLabel("纹理信息");
    setMinimumWidth(150);
    
    // 设置树状结构
    setRootIsDecorated(true);
    setAlternatingRowColors(true);
    
    // 创建右键菜单
    context_menu_ = new QMenu(this);
    
    add_action_ = new QAction("添加纹理...", this);
    connect(add_action_, &QAction::triggered, this, &TextureListWidget::onAddTexture);
    context_menu_->addAction(add_action_);
    
    context_menu_->addSeparator();
    
    details_action_ = new QAction("详细信息...", this);
    connect(details_action_, &QAction::triggered, this, &TextureListWidget::onShowDetails);
    context_menu_->addAction(details_action_);
    
    QAction* edit_texture_action_ = new QAction("纹理编辑器...", this);
    connect(edit_texture_action_, &QAction::triggered, this, &TextureListWidget::onEditTexture);
    context_menu_->addAction(edit_texture_action_);
    
    context_menu_->addSeparator();
    
    remove_action_ = new QAction("移除纹理", this);
    connect(remove_action_, &QAction::triggered, this, &TextureListWidget::onRemoveTexture);
    context_menu_->addAction(remove_action_);

    remove_path_action_ = new QAction("移除路径", this);
    connect(remove_path_action_, &QAction::triggered, this, &TextureListWidget::onRemovePath);
    context_menu_->addAction(remove_path_action_);
    
    edit_action_ = edit_texture_action_;  // 保持兼容性
    
    // 双击选择patch
    connect(this, &QTreeWidget::itemDoubleClicked, this, &TextureListWidget::onItemDoubleClicked);
}

TextureListWidget::~TextureListWidget() = default;

void TextureListWidget::addTexture(int patch_id, const TextureInfo& info)
{
    textures_[patch_id] = info;
    updateList();
    spdlog::info("添加纹理到patch {}: {}", patch_id, info.svg_filepath);
}

void TextureListWidget::removeTexture(int patch_id)
{
    auto it = textures_.find(patch_id);
    if (it != textures_.end()) {
        textures_.erase(it);
        updateList();
        spdlog::info("移除patch {} 的纹理", patch_id);
    }
}

void TextureListWidget::updateTexture(int patch_id, const TextureInfo& info)
{
    textures_[patch_id] = info;
    updateList();
    spdlog::info("更新patch {} 的纹理信息", patch_id);
}

TextureInfo TextureListWidget::getTexture(int patch_id) const
{
    auto it = textures_.find(patch_id);
    if (it != textures_.end()) {
        return it->second;
    }
    return TextureInfo();
}

void TextureListWidget::clearTextures()
{
    textures_.clear();
    updateList();
}

void TextureListWidget::setSelectedPatchId(int patch_id)
{
    if (selected_patch_id_ == patch_id) {
        return;
    }
    selected_patch_id_ = patch_id;
    updateList();
}

void TextureListWidget::updateList()
{
    clear();

    bool selected_patch_item_set = false;

    // 按patch_id分组（虽然每个patch只有一个纹理，但保持树状结构）
    for (const auto& pair : textures_) {
        int patch_id = pair.first;
        const TextureInfo& info = pair.second;

        // 创建patch节点
        QTreeWidgetItem* patch_item = new QTreeWidgetItem(this);
        QString patch_text = QString("Patch %1").arg(patch_id);
        if (patch_id == selected_patch_id_) {
            patch_text += " [当前选中]";
            selected_patch_item_set = true;
        }
        if (info.has_saved_path) {
            patch_text += " [已保存路径]";
        }
        patch_item->setText(0, patch_text);
        patch_item->setData(0, Qt::UserRole, patch_id);
        patch_item->setExpanded(true);

        // 创建纹理子节点
        QFileInfo file_info(QString::fromStdString(info.svg_filepath));
        QString texture_text = QString("%1 (%2x%3)")
            .arg(file_info.fileName())
            .arg(info.texture_width)
            .arg(info.texture_height);

        QTreeWidgetItem* texture_item = new QTreeWidgetItem(patch_item);
        texture_item->setText(0, texture_text);
        texture_item->setData(0, Qt::UserRole, patch_id);  // 也保存patch_id以便右键菜单使用
    }

    if (selected_patch_id_ >= 0 && !selected_patch_item_set) {
        QTreeWidgetItem* patch_item = new QTreeWidgetItem(this);
        patch_item->setText(0, QString("Patch %1 [当前选中, 未贴图]").arg(selected_patch_id_));
        patch_item->setData(0, Qt::UserRole, selected_patch_id_);
        patch_item->setExpanded(true);
    }
}

void TextureListWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QTreeWidgetItem* item = itemAt(event->pos());
    if (item) {
        setCurrentItem(item);
    }
    
    // 根据是否有选中项来启用/禁用菜单项
    bool has_item = (item != nullptr);
    details_action_->setEnabled(has_item);
    remove_action_->setEnabled(has_item);
    remove_path_action_->setEnabled(has_item);
    edit_action_->setEnabled(has_item);
    
    context_menu_->exec(event->globalPos());
}

void TextureListWidget::onItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    if (item) {
        int patch_id = item->data(0, Qt::UserRole).toInt();
        if (patch_id >= 0) {
            emit textureSelected(patch_id);
        }
    }
}

void TextureListWidget::onAddTexture()
{
    emit addTextureRequested();
}

void TextureListWidget::onRemoveTexture()
{
    int patch_id = getSelectedPatchId();
    if (patch_id >= 0) {
        emit removeTextureRequested(patch_id);
    }
}

void TextureListWidget::onEditTexture()
{
    int patch_id = getSelectedPatchId();
    if (patch_id >= 0) {
        emit editTextureRequested(patch_id);
    }
}

void TextureListWidget::onRemovePath()
{
    int patch_id = getSelectedPatchId();
    if (patch_id >= 0) {
        emit removePathRequested(patch_id);
    }
}

void TextureListWidget::onShowDetails()
{
    int patch_id = getSelectedPatchId();
    if (patch_id >= 0) {
        emit showDetailsRequested(patch_id);
    }
}

int TextureListWidget::getSelectedPatchId() const
{
    QTreeWidgetItem* item = currentItem();
    if (item) {
        int patch_id = item->data(0, Qt::UserRole).toInt();
        if (patch_id >= 0) {
            return patch_id;
        }
        // 如果是父节点，尝试从子节点获取
        if (item->childCount() > 0) {
            QTreeWidgetItem* child = item->child(0);
            patch_id = child->data(0, Qt::UserRole).toInt();
            if (patch_id >= 0) {
                return patch_id;
            }
        }
        // 如果是子节点，尝试从父节点获取
        QTreeWidgetItem* parent = item->parent();
        if (parent) {
            patch_id = parent->data(0, Qt::UserRole).toInt();
            if (patch_id >= 0) {
                return patch_id;
            }
        }
    }
    return -1;
}
