#ifndef PATH_PREVIEW_H
#define PATH_PREVIEW_H

#include <QWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QSet>
#include <QVector>
#include <QPoint>
#include <QHash>

namespace nbcam {
class LaserJob;
}

class PathPreview : public QWidget
{
    Q_OBJECT

public:
    explicit PathPreview(QWidget *parent = nullptr);
    ~PathPreview() override;

    void updateJob(nbcam::LaserJob* job);
    void setSelectedSegmentIds(const QSet<int>& segment_ids, bool notify = true);

signals:
    void highlightedSegmentsChanged(const QVector<int>& segment_ids);
    void focusSegmentRequested(int segment_id);
    void savePathRequested();

private slots:
    void onItemSelectionChanged();
    void onContextMenuRequested(const QPoint& pos);
    void onHeaderSectionClicked(int section);
    void onExportCsv();
    void onSavePath();
    void onApplyIdRangeFilter();
    void onClearIdRangeFilter();
    void onPathTypeFilterChanged(int index);

private:
    void setupUI();
    void clear();
    bool isSegmentItem(const QTreeWidgetItem* item) const;
    QVector<int> selectedSegmentIdsInViewOrder() const;
    void applyCurrentSort();
    bool segmentPassesIdRangeFilter(int segment_id) const;
    bool pathTypePassesFilter(const QString& path_type) const;
    
    QTreeWidget* tree_widget_;
    QLineEdit* id_from_edit_;
    QLineEdit* id_to_edit_;
    QComboBox* type_filter_combo_;
    QHash<int, QTreeWidgetItem*> segment_items_;
    nbcam::LaserJob* current_job_;
    QSet<int> selected_segment_ids_;
    int sort_column_;
    Qt::SortOrder sort_order_;
    bool has_id_range_filter_;
    int id_range_min_;
    int id_range_max_;
};

#endif // PATH_PREVIEW_H
