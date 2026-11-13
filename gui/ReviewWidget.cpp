#include "ReviewWidget.hpp"

#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

#include "Doorman.hpp"

namespace {

const char* const ENTRY_ID_PROPERTY = "reviewEntryId";

const char* const ENTRY_STYLE =
	"QFrame#reviewEntry { border: 1px solid palette(mid); border-radius: 6px; background: palette(base); }"
	"QFrame#reviewEntry[active=\"true\"] { border: 2px solid palette(highlight); }";

} // !namespace

ReviewWidget::ReviewWidget(Doorman& doorman, QWidget* parent)
	: QWidget(parent) {
	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(8, 8, 4, 8);
	setAutoFillBackground(true);

	auto* header = new QLabel(tr("Review"), this);
	QFont headerFont = header->font();
	headerFont.setBold(true);
	header->setFont(headerFont);
	layout->addWidget(header);

	scrollArea = new QScrollArea(this);
	scrollArea->setWidgetResizable(true);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scrollArea->setFrameShape(QFrame::NoFrame);

	container = new QWidget(scrollArea);
	container->setStyleSheet(ENTRY_STYLE);
	entryLayout = new QVBoxLayout(container);
	entryLayout->setContentsMargins(0, 0, 4, 0);
	entryLayout->setSpacing(8);
	entryLayout->addStretch(1);
	scrollArea->setWidget(container);
	container->setAutoFillBackground(false);
	scrollArea->viewport()->setAutoFillBackground(false);
	layout->addWidget(scrollArea, 1);

	setMinimumWidth(220);
	doorman.attach(*this);
}

void ReviewWidget::addEntry(ReviewEntryId id, const QString& title, const QString& text) {
	auto found = entries.find(id);
	if (found == entries.end()) {
		Entry entry;
		entry.frame = new QFrame(container);
		entry.frame->setObjectName("reviewEntry");
		auto* frameLayout = new QVBoxLayout(entry.frame);
		frameLayout->setContentsMargins(10, 8, 10, 8);
		frameLayout->setSpacing(4);

		entry.titleLabel = new QLabel(entry.frame);
		QFont titleFont = entry.titleLabel->font();
		titleFont.setBold(true);
		entry.titleLabel->setFont(titleFont);
		entry.titleLabel->setWordWrap(true);
		entry.titleLabel->setTextFormat(Qt::PlainText);

		entry.textLabel = new QLabel(entry.frame);
		entry.textLabel->setWordWrap(true);
		entry.textLabel->setTextFormat(Qt::MarkdownText);
		entry.textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

		frameLayout->addWidget(entry.titleLabel);
		frameLayout->addWidget(entry.textLabel);

		for (QWidget* widget : {static_cast<QWidget*>(entry.frame), static_cast<QWidget*>(entry.titleLabel), static_cast<QWidget*>(entry.textLabel)}) {
			widget->setProperty(ENTRY_ID_PROPERTY, QVariant::fromValue(id));
			widget->installEventFilter(this);
		}

		entryLayout->insertWidget(entryLayout->count() - 1, entry.frame);	//before the stretch
		found = entries.emplace(id, entry).first;
	}

	found->second.titleLabel->setText(title);
	found->second.titleLabel->setVisible(!title.isEmpty());
	found->second.textLabel->setText(text);
}

void ReviewWidget::removeEntry(ReviewEntryId id) {
	const auto found = entries.find(id);
	if (found == entries.end()) {
		return;
	}
	found->second.frame->deleteLater();
	entries.erase(found);
	if (activeEntry == id) {
		activeEntry.reset();
	}
}

void ReviewWidget::clearEntries() {
	for (auto& [id, entry] : entries) {
		entry.frame->deleteLater();
	}
	entries.clear();
	activeEntry.reset();
}

void ReviewWidget::focusEntry(ReviewEntryId id) {
	const auto found = entries.find(id);
	if (found == entries.end()) {
		return;
	}
	if (activeEntry) {
		const auto previous = entries.find(*activeEntry);
		if (previous != entries.end()) {
			setEntryActive(previous->second, false);
		}
	}
	setEntryActive(found->second, true);
	activeEntry = id;
	scrollArea->ensureWidgetVisible(found->second.frame);
}

void ReviewWidget::setPanelColor(const QColor& color) {
	QPalette colors = palette();
	colors.setColor(QPalette::Window, color);
	setPalette(colors);

	//re-resolve palette() references in the stylesheet
	container->setStyleSheet(QString());
	container->setStyleSheet(ENTRY_STYLE);
}

std::size_t ReviewWidget::getEntryCount() const {
	return entries.size();
}

bool ReviewWidget::eventFilter(QObject* watched, QEvent* event) {
	if (event->type() == QEvent::MouseButtonRelease) {
		const auto* mouseEvent = static_cast<QMouseEvent*>(event);
		const QVariant id = watched->property(ENTRY_ID_PROPERTY);
		const auto* label = qobject_cast<QLabel*>(watched);
		const bool isSelectingText = label != nullptr && label->hasSelectedText();
		if (id.isValid() && mouseEvent->button() == Qt::LeftButton && !isSelectingText) {
			emit entryActivated(id.value<ReviewEntryId>());
		}
	}
	return QWidget::eventFilter(watched, event);
}

void ReviewWidget::setEntryActive(Entry& entry, bool active) {
	entry.frame->setProperty("active", active);
	entry.frame->style()->unpolish(entry.frame);
	entry.frame->style()->polish(entry.frame);
}
