#include <QtWidgets/QApplication>
#include <QtDebug>
#include <QTimer>
#include <QTcpSocket>
#include <QNetworkInterface>
#include <QThread>
#include <QMutex>
#include <QDateTime>

#include <MarbleWidget.h>
#include <MarbleModel.h>

#include <GeoDataPlacemark.h>
#include <GeoDataTour.h>
#include <GeoDataDocument.h>
#include <GeoDataLineString.h>
#include <GeoDataTreeModel.h>

#include <GeoDataStyle.h>
#include <GeoDataIconStyle.h>
#include <GeoDataLineStyle.h>
#include <GeoDataPolyStyle.h>
#include <GeoPainter.h>

#include <RouteRequest.h>
#include <RoutingManager.h>

#include <MarbleDirs.h>

#include "connections.h"

#include "common.h"

using namespace Marble;

class WorkerThread : public QThread
{
	Q_OBJECT

public:
	GeoConnections conns;
	
	QMutex mutex;
	volatile bool isRunning;
	
private:
	bool readConnections()
	{
		qDebug() << "read connections";
		static GeoConnections sConns;
		sConns.update();

		QMutexLocker l(&mutex);
		this->conns = sConns;

#ifdef _DEBUG
		qDebug() << "UpdateConnections()";
#endif
		return sConns.connections.size() > 0;
	}

public:
	explicit WorkerThread(QObject *parent = 0)
		: QThread(parent)
	{
	}

	void start()
	{
		isRunning = true;
		QThread::start();
	}

	void run()
	{
		qDebug() << "run started";
		while (isRunning)
		{
			if (readConnections())
			{
				emit connectionsUpdated();
			}
			if (!isRunning)
				break;
			msleep(50);
		}
		qDebug() << "run ended";
	}
	// Define signal:
signals:
	void connectionsUpdated();
};

struct PlaceMarkData
{
	GeoDataPlacemark * placemark;
	QDateTime lastUsage;
};

class NetMarbleWidget : public MarbleWidget
{
	Q_OBJECT

	WorkerThread * worker;

	GeoDataDocument *document;
	GeoDataPlacemark *localhost;
	std::map<unsigned long, PlaceMarkData> placemarks;

public:
	NetMarbleWidget()
		: MarbleWidget()
	{
		worker = new WorkerThread(this);
		connect(worker, SIGNAL(connectionsUpdated()), SLOT(updateMap()));

		localhost = new GeoDataPlacemark("localhost");
		localhost->setVisualCategory(GeoDataFeature::Bookmark);
		localhost->setVisible(false);

		document = new GeoDataDocument;
		document->append(localhost);
		model()->treeModel()->addDocument(document);
	}

	void show()
	{
		MarbleWidget::show();
		if (!worker->isRunning)
		{
			worker->start();
		}
	}

	void paintConnection(GeoPainter* painter, const GeoConnections& conns, const GeoConnection& to, const QColor& color)
	{
		QColor black(0, 0, 0);
		QColor teal(0, 102, 102);

		painter->setPen(black);

		GeoDataCoordinates fromCoord(conns.localCoord.longitude, conns.localCoord.latitude, 0.0, GeoDataCoordinates::Degree);

		const std::set<GeoAddress>& toAddresses = to.addresses;
		GeoDataCoordinates toCoord(to.longitude, to.latitude, 0.0, GeoDataCoordinates::Degree);

		GeoDataLineString lineData(RespectLatitudeCircle | Tessellate);
		lineData << fromCoord << toCoord;

		painter->setPen(black);
		painter->drawPolyline(lineData);

		if (toAddresses.size() > 0)
		{
			std::string desc = conns.getDescription(to);
			//painter->setBackgroundMode(Qt::OpaqueMode);
			painter->setPen(teal);
			painter->drawAnnotation(toCoord, desc.c_str());
		}
	}
	
	virtual void customPaint(GeoPainter* painter)
	{
		QMutex& mutex = worker->mutex;
		if (mutex.tryLock())
		{
			const GeoConnections& conns = worker->conns;
			const std::vector<GeoConnection>& connections = conns.connections;
			
			size_t n = connections.size();
			for (size_t i = 0; i < n; i++)
			{
				paintConnection(painter, conns, connections[i], QColor(255, 0, 0));
			}

			mutex.unlock();
		}
	}

public slots:
	void updateMap()
	{
		const GeoConnections& conns = worker->conns;
		const std::vector<GeoConnection>& connections = conns.connections;

		if (conns.localCoord.isUndefined())
		{
			localhost->setVisible(false);
		}
		else
		{
			localhost->setCoordinate(conns.localCoord.longitude, conns.localCoord.latitude, 0.0, GeoDataCoordinates::Degree);
			if (conns.localAddress.country != NULL)
			{
				localhost->setCountryCode(conns.localAddress.country);
			}
			if (!localhost->isVisible())
			{
				centerOn(*localhost, true);
			}
			localhost->setVisible(true);
		}

		emit update();
	}
};


//int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
int main(int argc, char** argv)
{
	//QString dataDir = "D:\\workspace-cpp\\network-world\\marble\\data\\";
	QString dataDir = "data\\";
	Marble::MarbleDirs::setMarbleDataPath(dataDir);

	QApplication app(__argc, __argv);
	// Create a Marble QWidget without a parent
	NetMarbleWidget *mapWidget = new NetMarbleWidget();
	WorkerThread *worker = new WorkerThread(mapWidget);
	// Load the OpenStreetMap map
	mapWidget->setMapThemeId("earth/openstreetmap/openstreetmap.dgml");
	
	//InitConnections("D:\\workspace-cpp\\network-world\\GeoLiteCity.dat");
	InitConnections("GeoLiteCity.dat");
	
	
	mapWidget->setShowCities(false);
	mapWidget->show();

	return app.exec();
}

#define Q_MOC_OUTPUT_REVISION 67
#include "main.moc"
