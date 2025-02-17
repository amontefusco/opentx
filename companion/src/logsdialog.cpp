#include "logsdialog.h"
#include "appdata.h"
#include "ui_logsdialog.h"
#include "helpers.h"
#if defined WIN32 || !defined __GNUC__
#include <windows.h>
#else
#include <unistd.h>
#endif

LogsDialog::LogsDialog(QWidget *parent) :
  QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
  ui(new Ui::LogsDialog)
{
  csvlog.clear();

  ui->setupUi(this);
  setWindowIcon(CompanionIcon("logs.png"));

  plotLock=false;

  colors.append(Qt::green);
  colors.append(Qt::red);
  colors.append(Qt::yellow);
  colors.append(Qt::magenta);
  colors.append(Qt::cyan);
  colors.append(Qt::darkBlue);
  colors.append(Qt::darkGreen);
  colors.append(Qt::darkRed);
  colors.append(Qt::darkYellow);
  colors.append(Qt::darkMagenta);
  colors.append(Qt::darkCyan);
  colors.append(Qt::blue);
  pen.setWidthF(1.0);

  // create and prepare a plot title layout element
  QCPPlotTitle *title = new QCPPlotTitle(ui->customPlot);
  title->setText(tr("Telemetry logs"));

  // add it to the main plot layout
  ui->customPlot->plotLayout()->insertRow(0);
  ui->customPlot->plotLayout()->addElement(0, 0, title);
  ui->customPlot->setNoAntialiasingOnDrag(true);
  ui->customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectAxes | QCP::iSelectLegend | QCP::iSelectPlottables);

  axisRect = ui->customPlot->axisRect();
  axisRect->axis(QCPAxis::atBottom)->setLabel(tr("Time (hh:mm:ss)"));
  axisRect->axis(QCPAxis::atBottom)->setTickLabelType(QCPAxis::ltDateTime);
  axisRect->axis(QCPAxis::atBottom)->setDateTimeFormat("hh:mm:ss");
  QDateTime now = QDateTime::currentDateTime();
  axisRect->axis(QCPAxis::atBottom)->setRange(now.addSecs(-60*60*2).toTime_t(), now.toTime_t());
  axisRect->axis(QCPAxis::atLeft)->setTickLabels(false);
  axisRect->addAxis(QCPAxis::atLeft);
  axisRect->addAxis(QCPAxis::atRight);
  axisRect->axis(QCPAxis::atLeft, 1)->setVisible(false);
  axisRect->axis(QCPAxis::atRight, 1)->setVisible(false);

  QFont legendFont = font();
  legendFont.setPointSize(10);
  ui->customPlot->legend->setFont(legendFont);
  ui->customPlot->legend->setSelectedFont(legendFont);
  axisRect->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignLeft);

  rightLegend = new QCPLegend;
  axisRect->insetLayout()->addElement(rightLegend, Qt::AlignTop | Qt::AlignRight);
  rightLegend->setLayer("legend");
  rightLegend->setFont(legendFont);
  rightLegend->setSelectedFont(legendFont);
  rightLegend->setVisible(false);

  ui->customPlot->setAutoAddPlottableToLegend(false);

  QString path = g.gePath();
  if (path.isEmpty() || !QFile(path).exists()) {
    ui->mapsButton->hide();
  }

  // connect slot that ties some axis selections together (especially opposite axes):
  connect(ui->customPlot, SIGNAL(selectionChangedByUser()), this, SLOT(selectionChanged()));
  // connect slots that takes care that when an axis is selected, only that direction can be dragged and zoomed:
  connect(ui->customPlot, SIGNAL(mousePress(QMouseEvent*)), this, SLOT(mousePress()));
  connect(ui->customPlot, SIGNAL(mouseWheel(QWheelEvent*)), this, SLOT(mouseWheel()));

  // make left axes transfer its range to right axes:
  connect(axisRect->axis(QCPAxis::atLeft), SIGNAL(rangeChanged(QCPRange)), this, SLOT(yAxisChangeRanges(QCPRange)));

  // connect some interaction slots:
  connect(ui->customPlot, SIGNAL(titleDoubleClick(QMouseEvent*, QCPPlotTitle*)), this, SLOT(titleDoubleClick(QMouseEvent*, QCPPlotTitle*)));
  connect(ui->customPlot, SIGNAL(axisDoubleClick(QCPAxis*,QCPAxis::SelectablePart,QMouseEvent*)), this, SLOT(axisLabelDoubleClick(QCPAxis*,QCPAxis::SelectablePart)));
  connect(ui->customPlot, SIGNAL(legendDoubleClick(QCPLegend*,QCPAbstractLegendItem*,QMouseEvent*)), this, SLOT(legendDoubleClick(QCPLegend*,QCPAbstractLegendItem*)));
  connect(ui->FieldsTW, SIGNAL(itemSelectionChanged()), this, SLOT(plotLogs()));
  connect(ui->logTable, SIGNAL(itemSelectionChanged()), this, SLOT(plotLogs()));
  connect(ui->Reset_PB, SIGNAL(clicked()), this, SLOT(plotLogs()));
}

LogsDialog::~LogsDialog()
{
  delete ui;
}

void LogsDialog::titleDoubleClick(QMouseEvent *evt, QCPPlotTitle *title)
{
  // Set the plot title by double clicking on it
  bool ok;
  QString newTitle = QInputDialog::getText(this, tr("Plot Title Change"), tr("New plot title:"), QLineEdit::Normal, title->text(), &ok);
  if (ok) {
    title->setText(newTitle);
    ui->customPlot->replot();
  }
}

void LogsDialog::axisLabelDoubleClick(QCPAxis *axis, QCPAxis::SelectablePart part)
{
  // Set an axis label by double clicking on it
  if (part == QCPAxis::spAxisLabel) {
    // only react when the actual axis label is clicked, not tick label or axis backbone
    bool ok;
    QString newLabel = QInputDialog::getText(this, tr("Axis Label Change"), tr("New axis label:"), QLineEdit::Normal, axis->label(), &ok);
    if (ok) {
      axis->setLabel(newLabel);
      ui->customPlot->replot();
    }
  }
}

void LogsDialog::legendDoubleClick(QCPLegend *legend, QCPAbstractLegendItem *item)
{
  // Rename a graph by double clicking on its legend item
  if (item) {
    // only react if item was clicked (user could have clicked on border padding of legend where there is no item, then item is 0)
    QCPPlottableLegendItem *plItem = qobject_cast<QCPPlottableLegendItem*>(item);
    bool ok;
    QString newName = QInputDialog::getText(this, tr("Graph Name Change"), tr("New graph name:"), QLineEdit::Normal, plItem->plottable()->name(), &ok);
    if (ok) {
      plItem->plottable()->setName(newName);
      ui->customPlot->replot();
    }
  }
}

void LogsDialog::selectionChanged()
{
  /*
   normally, axis base line, axis tick labels and axis labels are selectable separately, but we want
   the user only to be able to select the axis as a whole, so we tie the selected states of the tick labels
   and the axis base line together. However, the axis label shall be selectable individually.

   The selection state of the left and right axes shall be synchronized as well as the state of the
   bottom and top axes.

   Further, we want to synchronize the selection of the graphs with the selection state of the respective
   legend item belonging to that graph. So the user can select a graph by either clicking on the graph itself
   or on its legend item.
  */

  if (plotLock) return;

  // handle bottom axis and tick labels as one selectable object:
  if (axisRect->axis(QCPAxis::atBottom)->selectedParts().testFlag(QCPAxis::spAxis) ||
    axisRect->axis(QCPAxis::atBottom)->selectedParts().testFlag(QCPAxis::spTickLabels))
  {
    axisRect->axis(QCPAxis::atBottom)->setSelectedParts(QCPAxis::spAxis |
      QCPAxis::spTickLabels);
  }
  // make left and right axes be selected synchronously,
  // and handle axis and tick labels as one selectable object:
  if (axisRect->axis(QCPAxis::atLeft)->selectedParts().testFlag(QCPAxis::spAxis) ||
    axisRect->axis(QCPAxis::atLeft)->selectedParts().testFlag(QCPAxis::spTickLabels) ||
    (
      axisRect->axis(QCPAxis::atRight)->visible() &&
      (axisRect->axis(QCPAxis::atRight)->selectedParts().testFlag(QCPAxis::spAxis) ||
      axisRect->axis(QCPAxis::atRight)->selectedParts().testFlag(QCPAxis::spTickLabels))
    ) || (
      axisRect->axis(QCPAxis::atLeft, 1)->visible() &&
      (axisRect->axis(QCPAxis::atLeft, 1)->selectedParts().testFlag(QCPAxis::spAxis) ||
      axisRect->axis(QCPAxis::atLeft, 1)->selectedParts().testFlag(QCPAxis::spTickLabels))
    ) || (
      axisRect->axis(QCPAxis::atRight)->visible() &&
      (axisRect->axis(QCPAxis::atRight, 1)->selectedParts().testFlag(QCPAxis::spAxis) ||
      axisRect->axis(QCPAxis::atRight, 1)->selectedParts().testFlag(QCPAxis::spTickLabels))
    )
  ) {
    axisRect->axis(QCPAxis::atLeft)->setSelectedParts(QCPAxis::spAxis |
      QCPAxis::spTickLabels);
    if (axisRect->axis(QCPAxis::atRight)->visible()) {
      axisRect->axis(QCPAxis::atRight)->setSelectedParts(QCPAxis::spAxis |
        QCPAxis::spTickLabels);
      if (axisRect->axis(QCPAxis::atLeft, 1)->visible()) {
        axisRect->axis(QCPAxis::atLeft, 1)->setSelectedParts(QCPAxis::spAxis |
          QCPAxis::spTickLabels);
        if (axisRect->axis(QCPAxis::atRight, 1)->visible()) {
          axisRect->axis(QCPAxis::atRight, 1)->setSelectedParts(QCPAxis::spAxis |
            QCPAxis::spTickLabels);
        }
      }
    }
  }

  // synchronize selection of graphs with selection of corresponding legend items:
  for (int i=0; i<ui->customPlot->graphCount(); ++i) {
    QCPGraph *graph = ui->customPlot->graph(i);
    QCPPlottableLegendItem *item = ui->customPlot->legend->itemWithPlottable(graph);
    if (item == NULL) item = rightLegend->itemWithPlottable(graph);
    if (item->selected() || graph->selected()) {
      item->setSelected(true);
      graph->setSelected(true);
    }
  }
}

QList<QStringList> LogsDialog::filterGePoints(const QList<QStringList> & input)
{
  QList<QStringList> result;

  int n = input.count();
  if (n == 0) {
    return result;
  }

  int gpscol = 0;
  for (int i=1; i<input.at(0).count(); i++) {
    if (input.at(0).at(i) == "GPS") {
      gpscol=i;
    }
  }
  if (gpscol == 0) {
    QMessageBox::critical(this, tr("Error: no GPS data not found"), 
      tr("The column containing GPS coordinates must be named \"GPS\".\n\n\
The columns for altitude \"GAlt\" and for speed \"GSpd\" are optional"));
    return result;
  }

  result.append(input.at(0));
  bool rangeSelected = ui->logTable->selectionModel()->selectedRows().length() > 0;

  GpsGlitchFilter glitchFilter;
  GpsLatLonFilter latLonFilter;

  for (int i = 1; i < n; i++) {
    if ((ui->logTable->item(i-1, 1)->isSelected() && rangeSelected) || !rangeSelected) {

      QStringList latlon = extractLatLon(input.at(i).at(gpscol));
      QString latitude = latlon[0];
      QString longitude = latlon[1];
      double flatitude = toDecimalCoordinate(latitude);
      double flongitude = toDecimalCoordinate(longitude);

      // glitch filter
      if ( glitchFilter.isGlitch(flatitude, flongitude) ) {
        // qDebug() << "filterGePoints(): GPS glitch detected at" << i << flatitude << flongitude;  
        continue;
      }

      // lat long pair filter
      if ( !latLonFilter.isValid(latitude, longitude) ) {
        // qDebug() << "filterGePoints(): Lat-Lon pair wrong, skipping at" << i << latitude << longitude;  
        continue;
      }

      // qDebug() << "point " << latitude << longitude;
      result.append(input.at(i));
    }
  }

  // qDebug() << "filterGePoints(): filtered from" << input.count() << "to " << result.count() << "points";
  return result;
}

void LogsDialog::exportToGoogleEarth()
{
  // filter data points
  QList<QStringList> dataPoints = filterGePoints(csvlog);
  int n = dataPoints.count(); // number of points to export
  if (n==0) return;

  int gpscol=0, altcol=0, speedcol=0;
  QSet<int> nondataCols;
  for (int i=1; i<dataPoints.at(0).count(); i++) {
    // Long,Lat,Course,GPS Speed,GPS Alt
    if (dataPoints.at(0).at(i) == "GPS") {
      gpscol=i;
    }
    if (dataPoints.at(0).at(i).contains("GAlt")) {
      altcol = i;
      nondataCols << i;
    }
    if (dataPoints.at(0).at(i).contains("GSpd")) {
      speedcol = i;
      nondataCols << i;
    }
  }

  if (gpscol==0 ) {
    return;
  }

  // qDebug() << "gpscol" << gpscol << "altcol" << altcol << "speedcol" << speedcol;
  QString geIconFilename = generateProcessUniqueTempFileName("track0.png");
  if (QFile::exists(geIconFilename)) {
    QFile::remove(geIconFilename);
  }
  QFile::copy(":/images/track0.png", geIconFilename);

  QString geFilename = generateProcessUniqueTempFileName("flight.kml");
  if (QFile::exists(geFilename)) {
    QFile::remove(geFilename);
  }
  QFile geFile(geFilename);
  if (!geFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this, tr("Error"),
        tr("Cannot write file %1:\n%2.")
        .arg(geFilename)
        .arg(geFile.errorString()));
    return;
  }

  QTextStream outputStream(&geFile);

  // file header
  outputStream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<kml xmlns=\"http://www.opengis.net/kml/2.2\" xmlns:gx=\"http://www.google.com/kml/ext/2.2\">\n";
  outputStream << "\t<Document>\n\t\t<name>" << logFilename << "</name>\n";
  outputStream << "\t\t<Style id=\"multiTrack_n\">\n\t\t\t<IconStyle>\n\t\t\t\t<Icon>\n\t\t\t\t\t<href>file://" << geIconFilename << "</href>\n\t\t\t\t</Icon>\n\t\t\t</IconStyle>\n\t\t\t<LineStyle>\n\t\t\t\t<color>991081f4</color>\n\t\t\t\t<width>6</width>\n\t\t\t</LineStyle>\n\t\t</Style>\n";
  outputStream << "\t\t<Style id=\"multiTrack_h\">\n\t\t\t<IconStyle>\n\t\t\t\t<scale>0</scale>\n\t\t\t\t<Icon>\n\t\t\t\t\t<href>file://" << geIconFilename << "</href>\n\t\t\t\t</Icon>\n\t\t\t</IconStyle>\n\t\t\t<LineStyle>\n\t\t\t\t<color>991081f4</color>\n\t\t\t\t<width>8</width>\n\t\t\t</LineStyle>\n\t\t</Style>\n";
  outputStream << "\t\t<StyleMap id=\"multiTrack\">\n\t\t\t<Pair>\n\t\t\t\t<key>normal</key>\n\t\t\t\t<styleUrl>#multiTrack_n</styleUrl>\n\t\t\t</Pair>\n\t\t\t<Pair>\n\t\t\t\t<key>highlight</key>\n\t\t\t\t<styleUrl>#multiTrack_h</styleUrl>\n\t\t\t</Pair>\n\t\t</StyleMap>\n";
  outputStream << "\t\t<Style id=\"lineStyle\">\n\t\t\t<LineStyle>\n\t\t\t\t<color>991081f4</color>\n\t\t\t\t<width>6</width>\n\t\t\t</LineStyle>\n\t\t</Style>\n";
  outputStream << "\t\t<Schema id=\"schema\">\n";
  outputStream << "\t\t\t<gx:SimpleArrayField name=\"GPSSpeed\" type=\"float\">\n\t\t\t\t<displayName>GPS Speed</displayName>\n\t\t\t</gx:SimpleArrayField>\n";
  
  // declare additional fields
  for (int i=0; i<dataPoints.at(0).count()-2; i++) {
    if (ui->FieldsTW->item(0,i)->isSelected() && !nondataCols.contains(i+2)) {
      QString origName = dataPoints.at(0).at(i+2);
      QString safeName = origName;
      safeName.replace(" ","_");
      outputStream << "\t\t\t<gx:SimpleArrayField name=\""<< safeName <<"\" ";
      outputStream << "type=\"string\"";   // additional fields have fixed type: string
      outputStream << ">\n\t\t\t\t<displayName>" << origName << "</displayName>\n\t\t\t</gx:SimpleArrayField>\n";
    }
  }

  QString planeName;
  if (logFilename.indexOf("-")>0) {
    planeName=logFilename.left(logFilename.indexOf("-"));
  } else {
    planeName=logFilename;
  }

  outputStream << "\t\t</Schema>\n";
  outputStream << "\t\t<Folder>\n\t\t\t<name>Log Data</name>\n\t\t\t<Placemark>\n\t\t\t\t<name>" << planeName << "</name>";
  outputStream << "\n\t\t\t\t<styleUrl>#multiTrack</styleUrl>";
  outputStream << "\n\t\t\t\t<gx:Track>\n";
  outputStream << "\n\t\t\t\t\t<altitudeMode>absolute</altitudeMode>\n";

  // time data points
  for (int i=1; i<n; i++) {
    QString tstamp=dataPoints.at(i).at(0)+QString("T")+dataPoints.at(i).at(1)+QString("Z");
    outputStream << "\t\t\t\t\t<when>"<< tstamp <<"</when>\n";
  }

  // coordinate data points
  for (int i=1; i<n; i++) {
    QStringList latlon = extractLatLon(dataPoints.at(i).at(gpscol));
    QString latitude = latlon[0];
    QString longitude = latlon[1];
    int altitude = altcol ? dataPoints.at(i).at(altcol).toFloat() : 0;
    latitude.sprintf("%3.8f", toDecimalCoordinate(latitude));
    longitude.sprintf("%3.8f", toDecimalCoordinate(longitude));
    outputStream << "\t\t\t\t\t<gx:coord>" << longitude << " " << latitude << " " <<  altitude << " </gx:coord>\n" ;
  }

  // additional data for data points
  outputStream << "\t\t\t\t\t<ExtendedData>\n\t\t\t\t\t\t<SchemaData schemaUrl=\"#schema\">\n";

  if (speedcol) {
    // gps speed data points
    outputStream << "\t\t\t\t\t\t\t<gx:SimpleArrayData name=\"GPSSpeed\">\n";
    for (int i=1; i<n; i++) {
      outputStream << "\t\t\t\t\t\t\t\t<gx:value>"<< dataPoints.at(i).at(speedcol) <<"</gx:value>\n";
    }
    outputStream << "\t\t\t\t\t\t\t</gx:SimpleArrayData>\n";
  }

  // add values for additional fields
  for (int i=0; i<dataPoints.at(0).count()-2; i++) {
    if (ui->FieldsTW->item(0,i)->isSelected() && !nondataCols.contains(i+2)) {
      QString safeName = dataPoints.at(0).at(i+2);;
      safeName.replace(" ","_");
      outputStream << "\t\t\t\t\t\t\t<gx:SimpleArrayData name=\""<< safeName <<"\">\n";
      for (int j=1; j<n; j++) {
        outputStream << "\t\t\t\t\t\t\t\t<gx:value>"<< dataPoints.at(j).at(i+2) <<"</gx:value>\n";
      }
      outputStream << "\t\t\t\t\t\t\t</gx:SimpleArrayData>\n";
    }
  }

  outputStream << "\t\t\t\t\t\t</SchemaData>\n\t\t\t\t\t</ExtendedData>\n\t\t\t\t</gx:Track>\n\t\t\t</Placemark>\n\t\t</Folder>\n\t</Document>\n</kml>";
  geFile.close();

  QString gePath = g.gePath();
  QStringList parameters;
#ifdef __APPLE__
  parameters << "-a";
  parameters << gePath;
  gePath = "/usr/bin/open";
#endif
  parameters << geFilename;
  QProcess *process = new QProcess(this);
  process->start(gePath, parameters);
}

void LogsDialog::on_mapsButton_clicked()
{
  ui->FieldsTW->setDisabled(true);
  ui->logTable->setDisabled(true);

  exportToGoogleEarth();

  ui->FieldsTW->setDisabled(false);
  ui->logTable->setDisabled(false);
}

void LogsDialog::mousePress()
{
  // if an axis is selected, only allow the direction of that axis to be dragged
  // if no axis is selected, both directions may be dragged

  if (axisRect->axis(QCPAxis::atBottom)->selectedParts().testFlag(QCPAxis::spAxis))
    axisRect->setRangeDrag(axisRect->axis(QCPAxis::atBottom)->orientation());
  else if (axisRect->axis(QCPAxis::atLeft)->selectedParts().testFlag(QCPAxis::spAxis))
    axisRect->setRangeDrag(axisRect->axis(QCPAxis::atLeft)->orientation());
  else
    axisRect->setRangeDrag(Qt::Horizontal | Qt::Vertical);
}

void LogsDialog::mouseWheel()
{
  // if an axis is selected, only allow the direction of that axis to be zoomed
  // if no axis is selected, both directions may be zoomed
  int orientation=0;
  if (ui->ZoomX_ChkB->isChecked()) {
    orientation|=Qt::Horizontal;
  }
  if (ui->ZoomY_ChkB->isChecked()) {
    orientation|=Qt::Vertical;
  }
  if (orientation) {
    axisRect->setRangeZoom((Qt::Orientation)orientation);
  } else {
    axisRect->setRangeZoom(Qt::Horizontal|Qt::Vertical);
  }
}

void LogsDialog::removeAllGraphs()
{
  ui->customPlot->clearGraphs();
  ui->customPlot->legend->setVisible(false);
  rightLegend->clearItems();
  rightLegend->setVisible(false);
  axisRect->axis(QCPAxis::atRight)->setSelectedParts(QCPAxis::spNone);
  axisRect->axis(QCPAxis::atRight)->setVisible(false);
  axisRect->axis(QCPAxis::atLeft)->setSelectedParts(QCPAxis::spNone);
  axisRect->axis(QCPAxis::atLeft)->setTickLabels(false);
  axisRect->axis(QCPAxis::atLeft, 1)->setVisible(false);
  axisRect->axis(QCPAxis::atLeft, 1)->setSelectedParts(QCPAxis::spNone);
  axisRect->axis(QCPAxis::atRight, 1)->setVisible(false);
  axisRect->axis(QCPAxis::atRight, 1)->setSelectedParts(QCPAxis::spNone);
  axisRect->axis(QCPAxis::atBottom)->setSelectedParts(QCPAxis::spNone);
  ui->customPlot->replot();
}

void LogsDialog::on_fileOpen_BT_clicked()
{
  QString fileName = QFileDialog::getOpenFileName(this, tr("Select your log file"), g.logDir());
  if (!fileName.isEmpty()) {
    g.logDir(fileName);
    ui->FileName_LE->setText(fileName);
    if (cvsFileParse()) {
      ui->FieldsTW->clear();
      ui->logTable->clear();
      ui->FieldsTW->setShowGrid(false);
      ui->FieldsTW->setContentsMargins(0,0,0,0);
      ui->FieldsTW->setRowCount(csvlog.at(0).count()-2);
      ui->FieldsTW->setColumnCount(1);
      ui->FieldsTW->setHorizontalHeaderLabels(QStringList(tr("Available fields")));
      ui->logTable->setSelectionBehavior(QAbstractItemView::SelectRows);
      for (int i=2; i<csvlog.at(0).count(); i++) {
        QTableWidgetItem* item= new QTableWidgetItem(csvlog.at(0).at(i));
        ui->FieldsTW->setItem(0,i-2,item);
      }
      ui->FieldsTW->resizeRowsToContents();
      ui->logTable->setColumnCount(csvlog.at(0).count());
      ui->logTable->setRowCount(csvlog.count()-1);
      ui->logTable->setHorizontalHeaderLabels(csvlog.at(0));

      QAbstractItemModel *model = ui->logTable->model();
      for (int i=1; i<csvlog.count(); i++) {
        for (int j=0; j<csvlog.at(0).count(); j++) {
          model->setData(model->index(i - 1, j, QModelIndex()), csvlog.at(i).at(j));
        }
      }

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
      ui->logTable->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#else
      ui->logTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#endif
      QVarLengthArray<int> sizes;
      for (int i = 0; i < ui->logTable->columnCount(); i++) {
        sizes.append(ui->logTable->columnWidth(i));
      }
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
      ui->logTable->horizontalHeader()->setResizeMode(QHeaderView::Interactive);
#else
      ui->logTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
#endif
      for (int i = 0; i < ui->logTable->columnCount(); i++) {
        ui->logTable->setColumnWidth(i, sizes.at(i));
      }
    }
  }
}

bool LogsDialog::cvsFileParse()
{
  QFile file(ui->FileName_LE->text());
  int errors=0;
  int lines=-1;

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { // reading HEX TEXT file
    return false;
  }
  else {
    csvlog.clear();
    logFilename.clear();
    QTextStream inputStream(&file);
    QString buffer = file.readLine();

    if (buffer.startsWith("Date,Time")) {
      file.reset();
    }
    else {
      return false;
    }

    int numfields=-1;
    while (!file.atEnd()) {
      QString line = file.readLine().trimmed();
      QStringList columns = line.split(',');
      if (numfields==-1) {
        numfields=columns.count();
      }
      if (columns.count()==numfields) {
        csvlog.append(columns);
      }
      else {
        errors++;
      }
      lines++;
    }

    logFilename = QFileInfo(file.fileName()).baseName();
  }

  file.close();
  if (errors > 1) {
    QMessageBox::warning(this, "Companion", tr("The selected logfile contains %1 invalid lines out of  %2 total lines").arg(errors).arg(lines));
  }

  int n = csvlog.count();
  if (n == 1) {
    csvlog.clear();
    return false;
  }

  plotLock = true;
  setFlightSessions();
  plotLock = false;

  return true;
}

struct FlightSession {
  QDateTime start;
  QDateTime end;
};

QDateTime LogsDialog::getRecordTimeStamp(int index) 
{
  QString tstamp = csvlog.at(index).at(0) + " " + csvlog.at(index).at(1);
  if (csvlog.at(index).at(1).contains(".")) 
    return QDateTime::fromString(tstamp, "yyyy-MM-dd HH:mm:ss.zzz");
  return QDateTime::fromString(tstamp, "yyyy-MM-dd HH:mm:ss");
}

QString LogsDialog::generateDuration(const QDateTime & start, const QDateTime & end)
{
  int secs = start.secsTo(end);
  QString durationString;
  if (secs >= 3600) {
    durationString = QString("%1:").arg(secs/3600); 
    secs %= 3600;
  }
  durationString += QString("%1:%2").arg(secs/60, 2, 10, QChar('0')).arg(secs%60, 2, 10, QChar('0'));  
  return durationString;
}

void LogsDialog::setFlightSessions()
{
  ui->sessions_CB->clear();

  int n = csvlog.count();
  // qDebug() << "records" << n;

  // find session breaks
  QList<int> sessions;
  QDateTime lastvalue;
  for (int i = 1; i < n; i++) {
    QDateTime tmp = getRecordTimeStamp(i);
    if (lastvalue.secsTo(tmp) > 60) {
      sessions.push_back(i-1);
      // qDebug() << "session index" << i-1;
    }
    lastvalue = tmp;
  }
  sessions.push_back(n-1);

  //now construct a list of sessions with their times
  //total time
  int noSesions = sessions.size()-1;
  QString label = QString("%1 ").arg(noSesions);
  label += tr(noSesions > 1 ? "sessions" : "session");
  label += " <" + tr("total duration ") + generateDuration(getRecordTimeStamp(1), getRecordTimeStamp(n-1)) + ">";
  ui->sessions_CB->addItem(label);

  // add individual sessions
  if (sessions.size() > 2) {
    for (int i = 1; i < sessions.size(); i++) {
      QDateTime sessionStart = getRecordTimeStamp(sessions.at(i-1)+1);
      QDateTime sessionEnd = getRecordTimeStamp(sessions.at(i));
      QString label = sessionStart.toString("HH:mm:ss") + " <" + tr("duration ") + generateDuration(sessionStart, sessionEnd) + ">";
      ui->sessions_CB->addItem(label, sessions.at(i-1));
      // qDebug() << "added label" << label << sessions.at(i-1);
    }
  }
}

void LogsDialog::on_sessions_CB_currentIndexChanged(int index)
{
  if (plotLock) return;
  plotLock = true;

  ui->logTable->clearSelection();

  if (index != 0) {
    int bottom;
    if (index < ui->sessions_CB->count() - 1) {
      bottom = ui->sessions_CB->itemData(index + 1, Qt::UserRole).toInt();
    } else {
      bottom = ui->logTable->rowCount();
    }

    QModelIndex topLeft = ui->logTable->model()->index(
      ui->sessions_CB->itemData(index, Qt::UserRole).toInt(), 0 , QModelIndex());
    QModelIndex bottomRight = ui->logTable->model()->index(
      bottom - 1, ui->logTable->columnCount() - 1, QModelIndex());

    QItemSelection selection(topLeft, bottomRight);
    ui->logTable->selectionModel()->select(selection, QItemSelectionModel::Select);
  }

  plotLock = false;
  plotLogs();
}

void LogsDialog::plotLogs()
{
  if (plotLock) return;

  if (!ui->FieldsTW->selectedItems().length()) {
    removeAllGraphs();
    return;
  }

  plotsCollection plots;

  QModelIndexList selection = ui->logTable->selectionModel()->selectedRows();
  int rowCount = selection.length();
  bool hasLogSelection;
  QVarLengthArray<int> selectedRows;

  if (rowCount) {
    hasLogSelection = true;
    foreach (QModelIndex index, selection) {
      selectedRows.append(index.row());
    }
    qSort(selectedRows.begin(), selectedRows.end());
  } else {
    hasLogSelection = false;
    rowCount = ui->logTable->rowCount();
  }

  plots.min_x = QDateTime::currentDateTime().toTime_t();
  plots.max_x = 0;

  foreach (QTableWidgetItem *plot, ui->FieldsTW->selectedItems()) {
    coords plotCoords;
    int plotColumn = plot->row() + 2; // Date and Time first

    plotCoords.min_y = INVALID_MIN;
    plotCoords.max_y = INVALID_MAX;
    plotCoords.yaxis = firstLeft;
    plotCoords.name = plot->text();

    for (int row = 0; row < rowCount; row++) {
      QTableWidgetItem *logValue;
      double y;
      double time;
      QString time_str;

      if (hasLogSelection) {
        logValue = ui->logTable->item(selectedRows.at(row), plotColumn);
        time_str = ui->logTable->item(selectedRows.at(row), 0)->text() +
          QString(" ") + ui->logTable->item(selectedRows.at(row), 1)->text();
      } else {
        logValue = ui->logTable->item(row, plotColumn);
        time_str = ui->logTable->item(row, 0)->text() + QString(" ") +
          ui->logTable->item(row, 1)->text();
      }

      y = logValue->text().toDouble();
      plotCoords.y.push_back(y);

      if (plotCoords.min_y > y) plotCoords.min_y = y;
      if (plotCoords.max_y < y) plotCoords.max_y = y;

      if (time_str.contains('.')) {
        time = QDateTime::fromString(time_str, "yyyy-MM-dd HH:mm:ss.zzz")
          .toTime_t();
        time += time_str.mid(time_str.indexOf('.')).toDouble();
      } else {
        time = QDateTime::fromString(time_str, "yyyy-MM-dd HH:mm:ss")
          .toTime_t();
      }
      plotCoords.x.push_back(time);

      if (plots.min_x > time) plots.min_x = time;
      if (plots.max_x < time) plots.max_x = time;
    }

    double range_inc = (plotCoords.max_y - plotCoords.min_y) / 100;
    if (range_inc == 0) range_inc = 1;
    plotCoords.max_y += range_inc;
    plotCoords.min_y -= range_inc;

    plots.coords.append(plotCoords);
  }

  yAxesRanges[firstLeft].min = plots.coords.at(0).min_y;
  yAxesRanges[firstLeft].max = plots.coords.at(0).max_y;
  for (int i = firstRight; i < AXES_LIMIT; i++) {
    yAxesRanges[i].min = INVALID_MIN;
    yAxesRanges[i].max = INVALID_MAX;
  }
  plots.tooManyRanges = false;

  for (int i = 1; i < plots.coords.size(); i++) {
    double actualRange = yAxesRanges[firstLeft].max - yAxesRanges[firstLeft].min;
    double thisRange = plots.coords.at(i).max_y - plots.coords.at(i).min_y;

    while (yAxesRanges[plots.coords.at(i).yaxis].max != INVALID_MAX &&
        (thisRange > actualRange * 1.3 || thisRange * 1.3 < actualRange ||
        plots.coords.at(i).min_y > yAxesRanges[plots.coords.at(i).yaxis].max ||
        plots.coords.at(i).max_y < yAxesRanges[plots.coords.at(i).yaxis].min)
      ) {

      switch (plots.coords[i].yaxis) {
        case firstLeft:
          plots.coords[i].yaxis = firstRight;
          break;
        case firstRight:
          plots.coords[i].yaxis = secondLeft;
          break;
        case secondLeft:
          plots.coords[i].yaxis = secondRight;
          break;
        case secondRight:
          plots.tooManyRanges = true;
          break;
        default:
          break;
      }
      if (plots.tooManyRanges) break;

      actualRange = yAxesRanges[plots.coords.at(i).yaxis].max
        - yAxesRanges[plots.coords.at(i).yaxis].min;
    }

    if (plots.tooManyRanges) {
      break;
    } else {
      if (plots.coords.at(i).min_y < yAxesRanges[plots.coords.at(i).yaxis].min) {
        yAxesRanges[plots.coords.at(i).yaxis].min = plots.coords.at(i).min_y;
      }
      if (plots.coords.at(i).max_y > yAxesRanges[plots.coords.at(i).yaxis].max) {
        yAxesRanges[plots.coords.at(i).yaxis].max = plots.coords.at(i).max_y;
      }
    }
  }

  if (plots.tooManyRanges) {
    yAxesRanges[firstLeft].max = 101;
    yAxesRanges[firstLeft].min = -1;
    yAxesRanges[firstRight].max = INVALID_MAX;
    yAxesRanges[firstRight].min = INVALID_MIN;
    yAxesRanges[secondLeft].max = INVALID_MAX;
    yAxesRanges[secondLeft].min = INVALID_MIN;
    yAxesRanges[secondRight].max = INVALID_MAX;
    yAxesRanges[secondRight].min = INVALID_MIN;

    for (int i = 0; i < plots.coords.size(); i++) {
      plots.coords[i].yaxis = firstLeft;

      double factor = 100 / (plots.coords.at(i).max_y - plots.coords.at(i).min_y);
      for (int j = 0; j < plots.coords.at(i).y.count(); j++) {
        plots.coords[i].y[j] = factor * (plots.coords.at(i).y.at(j) - plots.coords.at(i).min_y);
      }
    }
  } else {
    for (int i = firstRight; i < AXES_LIMIT; i++) {
      if (yAxesRanges[i].max == INVALID_MAX) break;

      yAxesRatios[i] = (yAxesRanges[i].max - yAxesRanges[i].min) /
        (yAxesRanges[firstLeft].max - yAxesRanges[firstLeft].min);
    }
  }

  removeAllGraphs();

  axisRect->axis(QCPAxis::atBottom)->setRange(plots.min_x, plots.max_x);

  axisRect->axis(QCPAxis::atLeft)->setRange(yAxesRanges[firstLeft].min,
    yAxesRanges[firstLeft].max);

  if (plots.tooManyRanges) {
    axisRect->axis(QCPAxis::atLeft)->setTickLabels(false);
  } else {
    axisRect->axis(QCPAxis::atLeft)->setTickLabels(true);
  }

  if (yAxesRanges[firstRight].max != INVALID_MAX) {
    axisRect->axis(QCPAxis::atRight)->setRange(yAxesRanges[firstRight].min,
      yAxesRanges[firstRight].max);
    axisRect->axis(QCPAxis::atRight)->setVisible(true);

    rightLegend->setVisible(true);

    if (yAxesRanges[secondLeft].max != INVALID_MAX) {
      axisRect->axis(QCPAxis::atLeft, 1)->setVisible(true);
      axisRect->axis(QCPAxis::atLeft, 1)->setRange(yAxesRanges[secondLeft].min,
        yAxesRanges[secondLeft].max);

      if (yAxesRanges[secondRight].max != INVALID_MAX) {
        axisRect->axis(QCPAxis::atRight, 1)->setVisible(true);
        axisRect->axis(QCPAxis::atRight, 1)->setRange(yAxesRanges[secondRight].min,
          yAxesRanges[secondRight].max);
      }
    }
  }

  for (int i = 0; i < plots.coords.size(); i++) {
    switch (plots.coords[i].yaxis) {
      case firstLeft:
        ui->customPlot->addGraph();
        if (yAxesRanges[secondLeft].max != INVALID_MAX) {
          ui->customPlot->graph(i)->setName(plots.coords.at(i).name + tr(" (L1)"));
        } else {
          ui->customPlot->graph(i)->setName(plots.coords.at(i).name);
        }
        ui->customPlot->legend->addItem(
          new QCPPlottableLegendItem(ui->customPlot->legend, ui->customPlot->graph(i)));
        break;
      case firstRight:
        ui->customPlot->addGraph(axisRect->axis(QCPAxis::atBottom),
          axisRect->axis(QCPAxis::atRight));
        if (yAxesRanges[secondRight].max != INVALID_MAX) {
          ui->customPlot->graph(i)->setName(plots.coords.at(i).name + tr(" (R1)"));
        } else {
          ui->customPlot->graph(i)->setName(plots.coords.at(i).name);
        }
        rightLegend->addItem(
          new QCPPlottableLegendItem(rightLegend, ui->customPlot->graph(i)));
        break;
      case secondLeft:
        ui->customPlot->addGraph(axisRect->axis(QCPAxis::atBottom),
          axisRect->axis(QCPAxis::atLeft, 1));
        ui->customPlot->graph(i)->setName(plots.coords.at(i).name + tr(" (L2)"));
        ui->customPlot->legend->addItem(
          new QCPPlottableLegendItem(ui->customPlot->legend, ui->customPlot->graph(i)));
        break;
      case secondRight:
        ui->customPlot->addGraph(axisRect->axis(QCPAxis::atBottom),
          axisRect->axis(QCPAxis::atRight, 1));
        ui->customPlot->graph(i)->setName(plots.coords.at(i).name + tr(" (R2)"));
        rightLegend->addItem(
          new QCPPlottableLegendItem(rightLegend, ui->customPlot->graph(i)));
        break;
      default:
        break;
    }

    ui->customPlot->graph(i)->setData(plots.coords.at(i).x,
      plots.coords.at(i).y);
    pen.setColor(colors.at(i % colors.size()));
    ui->customPlot->graph(i)->setPen(pen);
  }

  ui->customPlot->legend->setVisible(true);
  ui->customPlot->replot();
}

void LogsDialog::yAxisChangeRanges(QCPRange range)
{
  if (axisRect->axis(QCPAxis::atRight)->visible()) {
    double lowerChange = (range.lower - yAxesRanges[firstLeft].min) *
      yAxesRatios[firstRight];
    double upperChange = (range.upper - yAxesRanges[firstLeft].max) *
      yAxesRatios[firstRight];

    yAxesRanges[firstRight].min += lowerChange;
    yAxesRanges[firstRight].max += upperChange;
    axisRect->axis(QCPAxis::atRight)->setRange(yAxesRanges[firstRight].min,
      yAxesRanges[firstRight].max);

    if (axisRect->axisCount(QCPAxis::atLeft) == 2) {
      lowerChange = (range.lower - yAxesRanges[firstLeft].min) *
        yAxesRatios[secondLeft];
      upperChange = (range.upper - yAxesRanges[firstLeft].max) *
        yAxesRatios[secondLeft];

      yAxesRanges[secondLeft].min += lowerChange;
      yAxesRanges[secondLeft].max += upperChange;
      axisRect->axis(QCPAxis::atLeft, 1)->setRange(yAxesRanges[secondLeft].min,
        yAxesRanges[secondLeft].max);

      if (axisRect->axisCount(QCPAxis::atRight) == 2) {
        lowerChange = (range.lower - yAxesRanges[firstLeft].min) *
          yAxesRatios[secondRight];
        upperChange = (range.upper - yAxesRanges[firstLeft].max) *
          yAxesRatios[secondRight];

        yAxesRanges[secondRight].min += lowerChange;
        yAxesRanges[secondRight].max += upperChange;
        axisRect->axis(QCPAxis::atRight, 1)->setRange(yAxesRanges[secondRight].min,
          yAxesRanges[secondRight].max);
      }
    }

    yAxesRanges[firstLeft].min = range.lower;
    yAxesRanges[firstLeft].max = range.upper;
  }
}
