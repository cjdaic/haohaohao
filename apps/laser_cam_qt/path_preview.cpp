#include "path_preview.h"
#include "../core/job/laser_job.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidgetItem>
#include <QAbstractItemView>
#include <QHeaderView>
#include <QSignalBlocker>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QIntValidator>
#include <QStringConverter>
#include <algorithm>
#include <cmath>
#include <array>
#include <limits>

namespace {

constexpr int kSortKeyRole = Qt::UserRole + 2;

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

Vec3 operator-(const Vec3& a, const Vec3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

double dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

double norm(const Vec3& v)
{
    return std::sqrt(dot(v, v));
}

Vec3 normalized(const Vec3& v)
{
    const double n = norm(v);
    if (n <= 1e-12) {
        return {0.0, 0.0, 0.0};
    }
    return {v.x / n, v.y / n, v.z / n};
}

double distanceToLine(const Vec3& p, const Vec3& a, const Vec3& b)
{
    const Vec3 ab = b - a;
    const double ab_len = norm(ab);
    if (ab_len <= 1e-12) {
        return norm(p - a);
    }
    const Vec3 ap = p - a;
    return norm(cross(ap, ab)) / ab_len;
}

QString formatXYZ(const nbcam::PathPoint& p)
{
    return QString("(%1, %2, %3)")
        .arg(p.x, 0, 'f', 3)
        .arg(p.y, 0, 'f', 3)
        .arg(p.z, 0, 'f', 3);
}

QString formatUV(const nbcam::PathPoint& p)
{
    return QString("(%1, %2)")
        .arg(p.u, 0, 'f', 4)
        .arg(p.v, 0, 'f', 4);
}

bool solve3x3(std::array<std::array<double, 3>, 3> A,
              std::array<double, 3> b,
              std::array<double, 3>& x)
{
    for (int i = 0; i < 3; ++i) {
        int pivot = i;
        for (int r = i + 1; r < 3; ++r) {
            if (std::abs(A[r][i]) > std::abs(A[pivot][i])) {
                pivot = r;
            }
        }
        if (std::abs(A[pivot][i]) < 1e-12) {
            return false;
        }
        if (pivot != i) {
            std::swap(A[pivot], A[i]);
            std::swap(b[pivot], b[i]);
        }

        const double diag = A[i][i];
        for (int c = i; c < 3; ++c) {
            A[i][c] /= diag;
        }
        b[i] /= diag;

        for (int r = 0; r < 3; ++r) {
            if (r == i) {
                continue;
            }
            const double factor = A[r][i];
            if (std::abs(factor) < 1e-15) {
                continue;
            }
            for (int c = i; c < 3; ++c) {
                A[r][c] -= factor * A[i][c];
            }
            b[r] -= factor * b[i];
        }
    }

    x = b;
    return true;
}

QString classifyPathGeometry(const nbcam::PathSegment& segment)
{
    if (segment.type == nbcam::SegmentType::JUMP) {
        return "直线(跳转)";
    }
    if (segment.strategy == nbcam::FillStrategy::ARC_HATCH) {
        return "圆弧";
    }
    if (segment.strategy == nbcam::FillStrategy::CONTOUR) {
        return "轮廓";
    }
    return "直线";
}

int pathTypeSortRank(const QString& type_name)
{
    if (type_name == "轮廓") {
        return 0;
    }
    if (type_name == "直线") {
        return 1;
    }
    if (type_name == "直线(跳转)") {
        return 2;
    }
    if (type_name == "圆弧") {
        return 3;
    }
    return 99;
}

class SortableTreeItem : public QTreeWidgetItem
{
public:
    using QTreeWidgetItem::QTreeWidgetItem;

    bool operator<(const QTreeWidgetItem& other) const override
    {
        const QTreeWidget* tree = treeWidget();
        const int sort_col = tree ? tree->sortColumn() : 0;

        const QVariant left_key = data(sort_col, kSortKeyRole);
        const QVariant right_key = other.data(sort_col, kSortKeyRole);
        if (left_key.isValid() && right_key.isValid()) {
            const qlonglong left_num = left_key.toLongLong();
            const qlonglong right_num = right_key.toLongLong();
            if (left_num != right_num) {
                return left_num < right_num;
            }
        } else {
            const int text_cmp = QString::localeAwareCompare(text(sort_col), other.text(sort_col));
            if (text_cmp != 0) {
                return text_cmp < 0;
            }
        }

        const qlonglong left_id = data(0, kSortKeyRole).toLongLong();
        const qlonglong right_id = other.data(0, kSortKeyRole).toLongLong();
        if (left_id != right_id) {
            return left_id < right_id;
        }

        return QTreeWidgetItem::operator<(other);
    }
};

}  // namespace

PathPreview::PathPreview(QWidget *parent)
    : QWidget(parent)
    , id_from_edit_(nullptr)
    , id_to_edit_(nullptr)
    , type_filter_combo_(nullptr)
    , current_job_(nullptr)
    , sort_column_(0)
    , sort_order_(Qt::AscendingOrder)
    , has_id_range_filter_(false)
    , id_range_min_(0)
    , id_range_max_(0)
{
    setupUI();
}

PathPreview::~PathPreview() = default;

void PathPreview::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);

    QHBoxLayout* header_layout = new QHBoxLayout();
    QLabel* title = new QLabel("路径预览", this);
    title->setStyleSheet("font-weight: bold; font-size: 14px;");
    QPushButton* export_button = new QPushButton("导出CSV", this);
    QPushButton* save_button = new QPushButton("保存路径", this);
    export_button->setMinimumHeight(28);
    save_button->setMinimumHeight(28);
    header_layout->addWidget(title);
    header_layout->addStretch();
    header_layout->addWidget(save_button);
    header_layout->addWidget(export_button);
    layout->addLayout(header_layout);

    QHBoxLayout* filter_layout = new QHBoxLayout();
    QLabel* filter_label = new QLabel("ID范围:", this);
    id_from_edit_ = new QLineEdit(this);
    id_to_edit_ = new QLineEdit(this);
    id_from_edit_->setPlaceholderText("a");
    id_to_edit_->setPlaceholderText("b");
    id_from_edit_->setMaximumWidth(90);
    id_to_edit_->setMaximumWidth(90);
    id_from_edit_->setValidator(new QIntValidator(id_from_edit_));
    id_to_edit_->setValidator(new QIntValidator(id_to_edit_));
    QLabel* separator = new QLabel("到", this);
    QPushButton* apply_filter_button = new QPushButton("筛选", this);
    QPushButton* clear_filter_button = new QPushButton("重置", this);
    QLabel* type_filter_label = new QLabel("类型:", this);
    type_filter_combo_ = new QComboBox(this);
    type_filter_combo_->addItem("全部", "all");
    type_filter_combo_->addItem("轮廓", "轮廓");
    type_filter_combo_->addItem("直线", "直线");
    type_filter_combo_->addItem("圆弧", "圆弧");
    type_filter_combo_->addItem("跳转", "直线(跳转)");
    type_filter_combo_->setCurrentIndex(0);
    filter_layout->addWidget(filter_label);
    filter_layout->addWidget(id_from_edit_);
    filter_layout->addWidget(separator);
    filter_layout->addWidget(id_to_edit_);
    filter_layout->addSpacing(12);
    filter_layout->addWidget(type_filter_label);
    filter_layout->addWidget(type_filter_combo_);
    filter_layout->addWidget(apply_filter_button);
    filter_layout->addWidget(clear_filter_button);
    filter_layout->addStretch();
    layout->addLayout(filter_layout);
    
    tree_widget_ = new QTreeWidget(this);
    tree_widget_->setHeaderLabels(QStringList()
        << "段ID"
        << "路径类型"
        << "开关光"
        << "起点XYZ"
        << "终点XYZ"
        << "起点UV"
        << "终点UV"
        << "点数"
        << "长度(估算)");
    tree_widget_->setRootIsDecorated(true);
    tree_widget_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree_widget_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_widget_->setSortingEnabled(false);
    tree_widget_->header()->setSectionsClickable(true);
    tree_widget_->header()->setSortIndicatorShown(true);
    tree_widget_->header()->setSortIndicator(sort_column_, sort_order_);
    connect(tree_widget_, &QTreeWidget::itemSelectionChanged,
            this, &PathPreview::onItemSelectionChanged);
    connect(tree_widget_, &QTreeWidget::customContextMenuRequested,
            this, &PathPreview::onContextMenuRequested);
    connect(tree_widget_->header(), &QHeaderView::sectionClicked,
            this, &PathPreview::onHeaderSectionClicked);
    connect(export_button, &QPushButton::clicked,
            this, &PathPreview::onExportCsv);
    connect(save_button, &QPushButton::clicked,
            this, &PathPreview::onSavePath);
    connect(apply_filter_button, &QPushButton::clicked,
            this, &PathPreview::onApplyIdRangeFilter);
    connect(clear_filter_button, &QPushButton::clicked,
            this, &PathPreview::onClearIdRangeFilter);
    connect(id_from_edit_, &QLineEdit::returnPressed,
            this, &PathPreview::onApplyIdRangeFilter);
    connect(id_to_edit_, &QLineEdit::returnPressed,
            this, &PathPreview::onApplyIdRangeFilter);
    connect(type_filter_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PathPreview::onPathTypeFilterChanged);
    layout->addWidget(tree_widget_);
}

void PathPreview::updateJob(nbcam::LaserJob* job)
{
    current_job_ = job;
    const QSet<int> previous_selection = selected_segment_ids_;
    segment_items_.clear();
    clear();
    
    if (!job || job->segments.empty()) {
        if (!selected_segment_ids_.isEmpty()) {
            selected_segment_ids_.clear();
            emit highlightedSegmentsChanged(QVector<int>{});
        }
        return;
    }
    
    QSignalBlocker blocker(tree_widget_);
    tree_widget_->setUpdatesEnabled(false);
    QSet<int> visible_selection;

    for (const auto& segment : job->segments) {
        if (!segmentPassesIdRangeFilter(segment.id)) {
            continue;
        }
        const QString path_type = classifyPathGeometry(segment);
        if (!pathTypePassesFilter(path_type)) {
            continue;
        }

        SortableTreeItem* item = new SortableTreeItem();
        item->setText(0, QString::number(segment.id));
        item->setData(0, Qt::UserRole, segment.id);
        item->setData(0, Qt::UserRole + 1, true);
        item->setData(0, kSortKeyRole, segment.id);

        item->setText(1, path_type);
        item->setData(1, kSortKeyRole, pathTypeSortRank(path_type));

        const bool laser_on = (segment.type == nbcam::SegmentType::MARK);
        item->setText(2, laser_on ? "开光" : "关光");
        item->setData(2, kSortKeyRole, laser_on ? 0 : 1);

        if (!segment.points.empty()) {
            item->setText(3, formatXYZ(segment.points.front()));
            item->setText(4, formatXYZ(segment.points.back()));
            item->setText(5, formatUV(segment.points.front()));
            item->setText(6, formatUV(segment.points.back()));
        } else {
            item->setText(3, "-");
            item->setText(4, "-");
            item->setText(5, "-");
            item->setText(6, "-");
        }

        item->setText(7, QString::number(segment.points.size()));
        item->setData(7, kSortKeyRole, static_cast<qlonglong>(segment.points.size()));
        
        // 估算长度
        double length = 0.0;
        for (size_t i = 1; i < segment.points.size(); ++i) {
            const auto& p1 = segment.points[i - 1];
            const auto& p2 = segment.points[i];
            double dx = p2.x - p1.x;
            double dy = p2.y - p1.y;
            double dz = p2.z - p1.z;
            length += std::sqrt(dx * dx + dy * dy + dz * dz);
        }
        item->setText(8, QString::number(length, 'f', 2) + " mm");
        item->setData(8, kSortKeyRole, static_cast<qlonglong>(std::llround(length * 1000.0)));
        
        tree_widget_->addTopLevelItem(item);
        segment_items_.insert(segment.id, item);
        if (previous_selection.contains(segment.id)) {
            item->setSelected(true);
            visible_selection.insert(segment.id);
        }
    }
    
    for (int c = 0; c < tree_widget_->columnCount(); ++c) {
        tree_widget_->resizeColumnToContents(c);
    }
    applyCurrentSort();
    tree_widget_->setUpdatesEnabled(true);

    selected_segment_ids_ = visible_selection;
    emit highlightedSegmentsChanged(selectedSegmentIdsInViewOrder());
}

void PathPreview::setSelectedSegmentIds(const QSet<int>& segment_ids, bool notify)
{
    if (!tree_widget_) {
        return;
    }

    if (selected_segment_ids_ == segment_ids) {
        return;
    }

    QSignalBlocker blocker(tree_widget_);
    tree_widget_->setUpdatesEnabled(false);
    QSet<int> changed_segment_ids = selected_segment_ids_;
    changed_segment_ids.unite(segment_ids);
    for (int segment_id : changed_segment_ids) {
        QTreeWidgetItem* item = segment_items_.value(segment_id, nullptr);
        if (!item) {
            continue;
        }
        item->setSelected(segment_ids.contains(segment_id));
    }
    tree_widget_->setUpdatesEnabled(true);

    selected_segment_ids_ = segment_ids;
    if (notify) {
        emit highlightedSegmentsChanged(selectedSegmentIdsInViewOrder());
    }
}

void PathPreview::clear()
{
    segment_items_.clear();
    tree_widget_->clear();
}

bool PathPreview::isSegmentItem(const QTreeWidgetItem* item) const
{
    if (!item) {
        return false;
    }
    return item->data(0, Qt::UserRole + 1).toBool();
}

QVector<int> PathPreview::selectedSegmentIdsInViewOrder() const
{
    QVector<int> ids;
    const int top_level_count = tree_widget_->topLevelItemCount();
    ids.reserve(top_level_count);
    for (int i = 0; i < top_level_count; ++i) {
        QTreeWidgetItem* item = tree_widget_->topLevelItem(i);
        if (!item || !item->isSelected() || !isSegmentItem(item)) {
            continue;
        }
        bool ok = false;
        const int segment_id = item->data(0, Qt::UserRole).toInt(&ok);
        if (ok) {
            ids.push_back(segment_id);
        }
    }
    return ids;
}

void PathPreview::onItemSelectionChanged()
{
    QVector<int> ids = selectedSegmentIdsInViewOrder();
    QSet<int> new_selection;
    for (int id : ids) {
        new_selection.insert(id);
    }

    if (new_selection == selected_segment_ids_) {
        return;
    }

    selected_segment_ids_ = std::move(new_selection);
    emit highlightedSegmentsChanged(ids);
}

void PathPreview::onContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = tree_widget_->itemAt(pos);
    if (!item) {
        if (!selected_segment_ids_.isEmpty()) {
            tree_widget_->clearSelection();
        }
        return;
    }
    if (!isSegmentItem(item)) {
        return;
    }
    item->setSelected(!item->isSelected());

    bool ok = false;
    const int segment_id = item->data(0, Qt::UserRole).toInt(&ok);
    if (ok) {
        emit focusSegmentRequested(segment_id);
    }
}

void PathPreview::onHeaderSectionClicked(int section)
{
    if (section < 0 || section > 2) {
        return;
    }

    if (sort_column_ == section) {
        sort_order_ = (sort_order_ == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder;
    } else {
        sort_column_ = section;
        sort_order_ = Qt::AscendingOrder;
    }
    applyCurrentSort();
}

void PathPreview::applyCurrentSort()
{
    if (!tree_widget_) {
        return;
    }
    const int target_col = (sort_column_ >= 0 && sort_column_ <= 2) ? sort_column_ : 0;
    tree_widget_->sortItems(target_col, sort_order_);
    tree_widget_->header()->setSortIndicator(target_col, sort_order_);
}

void PathPreview::onExportCsv()
{
    if (!current_job_ || current_job_->segments.empty()) {
        QMessageBox::information(this, "导出CSV", "当前没有可导出的路径。");
        return;
    }

    const QString file_path = QFileDialog::getSaveFileName(
        this,
        "导出路径CSV",
        "",
        "CSV文件 (*.csv);;所有文件 (*.*)");
    if (file_path.isEmpty()) {
        return;
    }

    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this, "导出CSV", "无法写入文件。");
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true);  // 兼容 Windows Excel，避免中文乱码
    out << "segment_id,path_type,laser_state,point_index,u,v,x,y,z\n";

    for (const auto& segment : current_job_->segments) {
        const QString path_type = classifyPathGeometry(segment);
        const QString laser_state = (segment.type == nbcam::SegmentType::MARK) ? "开光" : "关光";
        for (size_t i = 0; i < segment.points.size(); ++i) {
            const auto& p = segment.points[i];
            out << segment.id << ","
                << "\"" << path_type << "\"" << ","
                << "\"" << laser_state << "\"" << ","
                << i << ","
                << QString::number(p.u, 'f', 6) << ","
                << QString::number(p.v, 'f', 6) << ","
                << QString::number(p.x, 'f', 6) << ","
                << QString::number(p.y, 'f', 6) << ","
                << QString::number(p.z, 'f', 6) << "\n";
        }
    }

    file.close();
    QMessageBox::information(this, "导出CSV", "路径CSV导出完成。");
}

void PathPreview::onSavePath()
{
    emit savePathRequested();
}

bool PathPreview::segmentPassesIdRangeFilter(int segment_id) const
{
    if (!has_id_range_filter_) {
        return true;
    }
    return segment_id >= id_range_min_ && segment_id <= id_range_max_;
}

bool PathPreview::pathTypePassesFilter(const QString& path_type) const
{
    if (!type_filter_combo_) {
        return true;
    }
    const QString type_key = type_filter_combo_->currentData().toString();
    if (type_key.isEmpty() || type_key == "all") {
        return true;
    }
    return path_type == type_key;
}

void PathPreview::onApplyIdRangeFilter()
{
    const QString from_text = id_from_edit_ ? id_from_edit_->text().trimmed() : QString();
    const QString to_text = id_to_edit_ ? id_to_edit_->text().trimmed() : QString();
    if (from_text.isEmpty() && to_text.isEmpty()) {
        onClearIdRangeFilter();
        return;
    }

    bool from_ok = false;
    bool to_ok = false;
    int from_id = from_text.toInt(&from_ok);
    int to_id = to_text.toInt(&to_ok);
    if (!from_ok || !to_ok) {
        QMessageBox::warning(this, "ID筛选", "请输入有效的ID区间 a,b（整数）。");
        return;
    }
    if (from_id > to_id) {
        std::swap(from_id, to_id);
    }

    has_id_range_filter_ = true;
    id_range_min_ = from_id;
    id_range_max_ = to_id;
    if (id_from_edit_) {
        id_from_edit_->setText(QString::number(id_range_min_));
    }
    if (id_to_edit_) {
        id_to_edit_->setText(QString::number(id_range_max_));
    }
    updateJob(current_job_);
}

void PathPreview::onClearIdRangeFilter()
{
    has_id_range_filter_ = false;
    id_range_min_ = 0;
    id_range_max_ = 0;
    if (id_from_edit_) {
        id_from_edit_->clear();
    }
    if (id_to_edit_) {
        id_to_edit_->clear();
    }
    if (type_filter_combo_) {
        type_filter_combo_->setCurrentIndex(0);
    }
    updateJob(current_job_);
}

void PathPreview::onPathTypeFilterChanged(int)
{
    updateJob(current_job_);
}
