#ifndef REVIEWWIDGET_HPP
#define REVIEWWIDGET_HPP

#include <map>
#include <optional>

#include <QWidget>

#include "GuiTypes.hpp"

class Doorman;
class QFrame;
class QLabel;
class QScrollArea;
class QVBoxLayout;

//Vertically scrollable list of review entries (title + Markdown text), in insertion order.
//Clicking an entry emits entryActivated(); what that means is up to the core.
class ReviewWidget : public QWidget {
	Q_OBJECT

public:
	explicit ReviewWidget(Doorman& doorman, QWidget* parent = nullptr);

	void addEntry(ReviewEntryId id, const QString& title, const QString& text);	//existing id: replaces content
	void removeEntry(ReviewEntryId id);
	void clearEntries();
	void focusEntry(ReviewEntryId id);
	std::size_t getEntryCount() const;

	void setPanelColor(const QColor& color);

signals:
	void entryActivated(ReviewEntryId id);

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
	struct Entry {
		QFrame* frame = nullptr;
		QLabel* titleLabel = nullptr;
		QLabel* textLabel = nullptr;
	};

	void setEntryActive(Entry& entry, bool active);

	QScrollArea* scrollArea = nullptr;
	QWidget* container = nullptr;
	QVBoxLayout* entryLayout = nullptr;
	std::map<ReviewEntryId, Entry> entries;
	std::optional<ReviewEntryId> activeEntry;
};

#endif
