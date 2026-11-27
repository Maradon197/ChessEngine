#ifndef SEARCHSETTINGSWIDGET_HPP
#define SEARCHSETTINGSWIDGET_HPP

#include <QString>
#include <QWidget>

class QLabel;
class QSlider;

//the engine's two search limits as sliders, shared by the analysis sidebar and the play view (each keeps its own values):
//	depth		plies the full search looks ahead, up to the horizon
//	quiescence	plies of captures the engine may still play out behind the horizon, so it doesn't stop mid-exchange; 0 = off
class SearchSettingsWidget : public QWidget {
	Q_OBJECT

public:
	//settingsPrefix: where the values are kept, e.g. "engine/analysis" -> ".../depth", ".../quiescence"
	SearchSettingsWidget(const QString& depthCaption, const QString& depthTip, const QString& settingsPrefix,
		int defaultDepth, QWidget* parent = nullptr);

	int getDepth() const { return depth; }
	int getQuiescenceDepth() const { return quiescenceDepth; }

signals:
	void depthChanged(int depth);
	void quiescenceDepthChanged(int depth);

private:
	QSlider* addSlider(const QString& caption, const QString& tip, int minimum, int maximum, QLabel*& value);

	QString settingsPrefix;
	QLabel* depthValue = nullptr;
	QLabel* quiescenceValue = nullptr;
	int depth = 1;
	int quiescenceDepth = 0;
};

#endif
