#include "telemetry.h"
#include "ui_telemetry.h"
#include "ui_telemetry_analog.h"
#include "ui_telemetry_customscreen.h"
#include "ui_telemetry_sensor.h"
#include "helpers.h"
#include "appdata.h"

TelemetryAnalog::TelemetryAnalog(QWidget *parent, FrSkyChannelData & analog, ModelData & model, GeneralSettings & generalSettings, Firmware * firmware):
  ModelPanel(parent, model, generalSettings, firmware),
  ui(new Ui::TelemetryAnalog),
  analog(analog),
  lock(false)
{
  ui->setupUi(this);

  float ratio = analog.getRatio();

  if (analog.type==0 || analog.type==1 || analog.type==2) {
    ui->RatioSB->setDecimals(1);
    ui->RatioSB->setMaximum(25.5*firmware->getCapability(TelemetryMaxMultiplier));
  }
  else {
    ui->RatioSB->setDecimals(0);
    ui->RatioSB->setMaximum(255*firmware->getCapability(TelemetryMaxMultiplier));
  }
  ui->RatioSB->setValue(ratio);

  update();

  ui->UnitCB->setCurrentIndex(analog.type);
  if (!IS_TARANIS(firmware->getBoard())) {
    ui->alarm1LevelCB->setCurrentIndex(analog.alarms[0].level);
    ui->alarm1GreaterCB->setCurrentIndex(analog.alarms[0].greater);
    ui->alarm2LevelCB->setCurrentIndex(analog.alarms[1].level);
    ui->alarm2GreaterCB->setCurrentIndex(analog.alarms[1].greater);
  }
  else {
    ui->alarm1LevelCB->hide();
    ui->alarm2LevelCB->hide();
    ui->alarm1GreaterCB->hide();
    ui->alarm2GreaterCB->hide();
    ui->alarm1Label->setText(tr("Low Alarm"));
    ui->alarm2Label->setText(tr("Critical Alarm"));
  }

  if (!(firmware->getCapability(Telemetry) & TM_HASOFFSET)) {
    ui->CalibSB->hide();
    ui->CalibLabel->hide();
  }
  else {
    ui->label_Max->setText(tr("Range"));
  }

  disableMouseScrolling();
}

void TelemetryAnalog::update()
{
  float ratio = analog.getRatio();
  float step = ratio / 255;
  float mini = (ratio * analog.offset) / 255;
  float maxi = mini + ratio;

  ui->alarm1ValueSB->setDecimals(2);
  ui->alarm1ValueSB->setSingleStep(step);
  ui->alarm1ValueSB->setMinimum(mini);
  ui->alarm1ValueSB->setMaximum(maxi);
  ui->alarm1ValueSB->setValue(mini + step*analog.alarms[0].value);

  ui->alarm2ValueSB->setDecimals(2);
  ui->alarm2ValueSB->setSingleStep(step);
  ui->alarm2ValueSB->setMinimum(mini);
  ui->alarm2ValueSB->setMaximum(maxi);
  ui->alarm2ValueSB->setValue(mini + step*analog.alarms[1].value);

  ui->CalibSB->setDecimals(2);
  ui->CalibSB->setMaximum(step*127);
  ui->CalibSB->setMinimum(-step*128);
  ui->CalibSB->setSingleStep(step);
  ui->CalibSB->setValue(mini);
}

void TelemetryAnalog::on_UnitCB_currentIndexChanged(int index)
{
    float ratio = analog.getRatio();
    analog.type = index;
    switch (index) {
      case 0:
      case 1:
      case 2:
        ui->RatioSB->setDecimals(1);
        ui->RatioSB->setMaximum(25.5*firmware->getCapability(TelemetryMaxMultiplier));
        break;
      default:
        ui->RatioSB->setDecimals(0);
        ui->RatioSB->setMaximum(255*firmware->getCapability(TelemetryMaxMultiplier));
        break;
    }
    ui->RatioSB->setValue(ratio);
    update();
    emit modified();
}

void TelemetryAnalog::on_RatioSB_valueChanged()
{
  if (!lock) {
    if (analog.type==0 || analog.type==1 || analog.type==2) {
      analog.multiplier = findmult(ui->RatioSB->value(), 25.5);
      float singlestep =(1<<analog.multiplier)/10.0;
      lock=true;
      ui->RatioSB->setSingleStep(singlestep);
      ui->RatioSB->setValue(round(ui->RatioSB->value()/singlestep)*singlestep);
      lock=false;
    }
    else {
      analog.multiplier = findmult(ui->RatioSB->value(), 255);
      float singlestep = (1<<analog.multiplier);
      lock = true;
      ui->RatioSB->setSingleStep(singlestep);
      ui->RatioSB->setValue(round(ui->RatioSB->value()/singlestep)*singlestep);
      lock = false;
    }
    emit modified();
  }
}

void TelemetryAnalog::on_RatioSB_editingFinished()
{
  if (!lock) {
    float ratio, calib, alarm1value,alarm2value;

    if (analog.type==0 || analog.type==1 || analog.type==2) {
      analog.multiplier = findmult(ui->RatioSB->value(), 25.5);
      ui->CalibSB->setSingleStep((1<<analog.multiplier)/10.0);
      ui->alarm1ValueSB->setSingleStep((1<<analog.multiplier)/10.0);
      ui->alarm2ValueSB->setSingleStep((1<<analog.multiplier)/10.0);
      analog.ratio = ((int)(round(ui->RatioSB->value()*10))/(1 <<analog.multiplier));
    }
    else {
      analog.multiplier = findmult(ui->RatioSB->value(), 255);
      ui->CalibSB->setSingleStep(1<<analog.multiplier);
      ui->alarm1ValueSB->setSingleStep(1<<analog.multiplier);
      ui->alarm2ValueSB->setSingleStep(1<<analog.multiplier);
      analog.ratio = (ui->RatioSB->value()/(1 << analog.multiplier));
    }
    ui->CalibSB->setMaximum((ui->RatioSB->value()*127)/255);
    ui->CalibSB->setMinimum((ui->RatioSB->value()*-128)/255);
    ui->alarm1ValueSB->setMaximum(ui->RatioSB->value());
    ui->alarm2ValueSB->setMaximum(ui->RatioSB->value());
    repaint();
    ratio=analog.ratio * (1 << analog.multiplier);
    calib=ui->CalibSB->value();
    alarm1value=ui->alarm1ValueSB->value();
    alarm2value=ui->alarm2ValueSB->value();
    if (analog.type==0) {
      calib*=10;
      alarm1value*=10;
      alarm2value*=10;
    }
    if (calib>0) {
      if (calib>((ratio*127)/255)) {
        analog.offset=127;
      }
      else {
        analog.offset=round(calib*255/ratio);
      }
    }
    if (calib<0) {
      if (calib<((ratio*-128)/255)) {
        analog.offset=-128;
      }
      else {
        analog.offset=round(calib*255/ratio);
      }
    }
    analog.alarms[0].value=round((alarm1value*255-analog.offset*(analog.ratio<<analog.multiplier))/(analog.ratio<<analog.multiplier));
    analog.alarms[1].value=round((alarm2value*255-analog.offset*(analog.ratio<<analog.multiplier))/(analog.ratio<<analog.multiplier));
    update();
    emit modified();
  }
}

void TelemetryAnalog::on_CalibSB_editingFinished()
{
    float ratio = analog.getRatio();
    float calib,alarm1value,alarm2value;

    if (ratio!=0) {
      analog.offset = round((255*ui->CalibSB->value()/ratio));
      calib=ratio*analog.offset/255.0;
      alarm1value=ui->alarm1ValueSB->value();
      alarm2value=ui->alarm2ValueSB->value();
      if (alarm1value<calib) {
        alarm1value=calib;
      }
      else if (alarm1value>(ratio+calib)) {
        alarm1value=ratio+calib;
      }
      if (alarm2value<calib) {
        alarm2value=calib;
      }
      else if (alarm2value>(ratio+calib)) {
        alarm2value=ratio+calib;
      }
      analog.alarms[0].value=round(((alarm1value-calib)*255)/ratio);
      analog.alarms[1].value=round(((alarm2value-calib)*255)/ratio);
    }
    else {
      analog.offset=0;
      analog.alarms[0].value=0;
      analog.alarms[1].value=0;
    }
    update();
    emit modified();
}

void TelemetryAnalog::on_alarm1LevelCB_currentIndexChanged(int index)
{
  analog.alarms[0].level = index;
  emit modified();
}


void TelemetryAnalog::on_alarm1GreaterCB_currentIndexChanged(int index)
{
  analog.alarms[0].greater = index;
  emit modified();
}

void TelemetryAnalog::on_alarm1ValueSB_editingFinished()
{
    float ratio = analog.getRatio();
    float calib, alarm1value;

    calib=analog.offset;
    alarm1value=ui->alarm1ValueSB->value();

    if (alarm1value<((calib*ratio)/255)) {
      analog.alarms[0].value=0;
    }
    else if (alarm1value>(ratio+(calib*ratio)/255)) {
      analog.alarms[0].value=255;
    }
    else {
      analog.alarms[0].value = round((alarm1value-((calib*ratio)/255))/ratio*255);
    }
    update();
    emit modified();
}

void TelemetryAnalog::on_alarm2LevelCB_currentIndexChanged(int index)
{
  analog.alarms[1].level = index;
  emit modified();
}

void TelemetryAnalog::on_alarm2GreaterCB_currentIndexChanged(int index)
{
  analog.alarms[1].greater = index;
  emit modified();
}

void TelemetryAnalog::on_alarm2ValueSB_editingFinished()
{
    float calib, alarm2value;
    float ratio = analog.getRatio();
    calib = analog.offset;
    alarm2value = ui->alarm2ValueSB->value();
    if (alarm2value<((calib*ratio)/255)) {
      analog.alarms[1].value=0;
    }
    else if (alarm2value>(ratio+(calib*ratio)/255)) {
      analog.alarms[1].value=255;
    }
    else {
      analog.alarms[1].value = round((alarm2value-((calib*ratio)/255))/ratio*255);
    }
    update();
    emit modified();
}

TelemetryAnalog::~TelemetryAnalog()
{
  delete ui;
}

/******************************************************/

TelemetryCustomScreen::TelemetryCustomScreen(QWidget *parent, ModelData & model, FrSkyScreenData & screen, GeneralSettings & generalSettings, Firmware * firmware):
  ModelPanel(parent, model, generalSettings, firmware),
  ui(new Ui::TelemetryCustomScreen),
  screen(screen)
{
  ui->setupUi(this);

  for (int l=0; l<4; l++) {
    for (int c=0; c<firmware->getCapability(TelemetryCustomScreensFieldsPerLine); c++) {
      fieldsCB[l][c] = new QComboBox(this);
      fieldsCB[l][c]->setProperty("index", c + (l<<8));
      ui->screenNumsLayout->addWidget(fieldsCB[l][c], l, c, 1, 1);
      connect(fieldsCB[l][c], SIGNAL(currentIndexChanged(int)), this, SLOT(customFieldChanged(int)));
    }
  }

  for (int l=0; l<4; l++) {
    barsCB[l] = new QComboBox(this);
    barsCB[l]->setProperty("index", l);
    connect(barsCB[l], SIGNAL(currentIndexChanged(int)), this, SLOT(barSourceChanged(int)));
    ui->screenBarsLayout->addWidget(barsCB[l], l, 0, 1, 1);

    minSB[l] = new QDoubleSpinBox(this);
    minSB[l]->setProperty("index", l);
    connect(minSB[l], SIGNAL(valueChanged(double)), this, SLOT(barMinChanged(double)));
    ui->screenBarsLayout->addWidget(minSB[l], l, 1, 1, 1);

    QLabel * label = new QLabel(this);
    label->setAutoFillBackground(false);
    label->setStyleSheet(QString::fromUtf8("Background:qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 rgba(0, 0, 128, 255), stop:0.339795 rgba(0, 0, 128, 255), stop:0.339799 rgba(255, 255, 255, 255), stop:0.662444 rgba(255, 255, 255, 255),)\n"""));
    label->setFrameShape(QFrame::Panel);
    label->setFrameShadow(QFrame::Raised);
    label->setAlignment(Qt::AlignCenter);
    ui->screenBarsLayout->addWidget(label, l, 2, 1, 1);

    maxSB[l] = new QDoubleSpinBox(this);
    maxSB[l]->setProperty("index", l);
    connect(maxSB[l], SIGNAL(valueChanged(double)), this, SLOT(barMaxChanged(double)));
    ui->screenBarsLayout->addWidget(maxSB[l], l, 3, 1, 1);
  }

  disableMouseScrolling();

  lock = true;
  if (IS_ARM(firmware->getBoard()))
    ui->screenType->addItem(tr("None"), TELEMETRY_SCREEN_NONE);
  ui->screenType->addItem(tr("Numbers"), TELEMETRY_SCREEN_NUMBERS);
  ui->screenType->addItem(tr("Bars"), TELEMETRY_SCREEN_BARS);
  if (IS_TARANIS(firmware->getBoard()))
    ui->screenType->addItem(tr("Script"), TELEMETRY_SCREEN_SCRIPT);
  ui->screenType->setField(screen.type, this);
  lock = false;

  if (IS_TARANIS(firmware->getBoard())) {
    QSet<QString> scriptsSet = getFilesSet(g.profile[g.id()].sdPath() + "/SCRIPTS/TELEMETRY", QStringList() << "*.lua", 8);
    populateFileComboBox(ui->scriptName, scriptsSet, screen.body.script.filename);
    connect(ui->scriptName, SIGNAL(currentIndexChanged(int)), this, SLOT(scriptNameEdited()));
    connect(ui->scriptName, SIGNAL(editTextChanged ( const QString)), this, SLOT(scriptNameEdited()));
  }

  update();
}

void TelemetryCustomScreen::populateTelemetrySourceCB(QComboBox * b, RawSource & source, bool last)
{
  if (IS_ARM(firmware->getBoard())) {
    populateSourceCB(b, source, generalSettings, model, POPULATE_NONE | POPULATE_SOURCES | POPULATE_SCRIPT_OUTPUTS | POPULATE_VIRTUAL_INPUTS | POPULATE_TRIMS | POPULATE_SWITCHES | POPULATE_TELEMETRY | (firmware->getCapability(GvarsInCS) ? POPULATE_GVARS : 0));
  }
  else {
    populateSourceCB(b, source, generalSettings, model, POPULATE_NONE | POPULATE_TELEMETRY | (last ? POPULATE_TELEMETRYEXT : 0));
  }
}

TelemetryCustomScreen::~TelemetryCustomScreen()
{
  delete ui;
}

void TelemetryCustomScreen::update()
{
  lock = true;

  ui->scriptName->setVisible(screen.type == TELEMETRY_SCREEN_SCRIPT);
  ui->screenNums->setVisible(screen.type == TELEMETRY_SCREEN_NUMBERS);
  ui->screenBars->setVisible(screen.type == TELEMETRY_SCREEN_BARS);

  for (int l=0; l<4; l++) {
    for (int c=0; c<firmware->getCapability(TelemetryCustomScreensFieldsPerLine); c++) {
      populateTelemetrySourceCB(fieldsCB[l][c], screen.body.lines[l].source[c], l==3);
    }
  }

  for (int l=0; l<4; l++) {
    populateTelemetrySourceCB(barsCB[l], screen.body.bars[l].source);
  }

  if (screen.type == TELEMETRY_SCREEN_BARS) {
    for (int i=0; i<4; i++) {
      updateBar(i);
    }
  }

  lock = false;
}

void TelemetryCustomScreen::updateBar(int line)
{
  lock = true;

  RawSource source = screen.body.bars[line].source;
  if (source.type != SOURCE_TYPE_NONE) {
    RawSourceRange range = source.getRange(model, generalSettings, RANGE_SINGLE_PRECISION);
    if (!IS_ARM(GetCurrentFirmware()->getBoard())) {
      int max = round((range.max - range.min) / range.step);
      if (int(255-screen.body.bars[line].barMax) > max) {
        screen.body.bars[line].barMax = 255 - max;
      }
    }
    minSB[line]->setEnabled(true);
    minSB[line]->setDecimals(range.decimals);
    minSB[line]->setMinimum(range.min);
    minSB[line]->setMaximum(range.max);
    minSB[line]->setSingleStep(range.step);
    minSB[line]->setValue(range.getValue(screen.body.bars[line].barMin));
    maxSB[line]->setEnabled(true);
    maxSB[line]->setDecimals(range.decimals);
    maxSB[line]->setMinimum(range.min);
    maxSB[line]->setMaximum(range.max);
    maxSB[line]->setSingleStep(range.step);
    if (IS_ARM(GetCurrentFirmware()->getBoard())) {
      maxSB[line]->setValue(range.getValue(screen.body.bars[line].barMax));
    }
    else {
      maxSB[line]->setValue(range.getValue(255 - screen.body.bars[line].barMax));
    }
  }
  else {
    minSB[line]->setDisabled(true);
    maxSB[line]->setDisabled(true);
  }

  lock = false;
}

void TelemetryCustomScreen::on_screenType_currentIndexChanged(int index)
{
  if (!lock) {
    memset(&screen.body, 0, sizeof(screen.body));
    update();
    emit modified();
  }
}

void TelemetryCustomScreen::scriptNameEdited()
{
  if (!lock) {
    lock = true;
    getFileComboBoxValue(ui->scriptName, screen.body.script.filename, 8);
    emit modified();
    lock = false;
  }
}

void TelemetryCustomScreen::customFieldChanged(int value)
{
  if (!lock) {
    int index = sender()->property("index").toInt();
    screen.body.lines[index/256].source[index%256] = RawSource(((QComboBox *)sender())->itemData(value).toInt());
    emit modified();
  }
}

void TelemetryCustomScreen::barSourceChanged(int value)
{
  if (!lock) {
    QComboBox * cb = qobject_cast<QComboBox*>(sender());
    int index = cb->property("index").toInt();
    screen.body.bars[index].source = RawSource(((QComboBox *)sender())->itemData(value).toInt());
    screen.body.bars[index].barMin = 0;
    screen.body.bars[index].barMax = 0;
    updateBar(index);
    emit modified();
  }
}

void TelemetryCustomScreen::barMinChanged(double value)
{
  if (!lock) {
    int line = sender()->property("index").toInt();
    if (IS_ARM(GetCurrentFirmware()->getBoard()))
      screen.body.bars[line].barMin = round(value / minSB[line]->singleStep());
    else
      screen.body.bars[line].barMin = round((value-minSB[line]->minimum()) / minSB[line]->singleStep());
    // TODO set min (maxSB)
    emit modified();
  }
}

void TelemetryCustomScreen::barMaxChanged(double value)
{
  if (!lock) {
    int line = sender()->property("index").toInt();
    if (IS_ARM(GetCurrentFirmware()->getBoard()))
      screen.body.bars[line].barMax = round((value) / maxSB[line]->singleStep());
    else
      screen.body.bars[line].barMax = 255 - round((value-minSB[line]->minimum()) / maxSB[line]->singleStep());
    // TODO set max (minSB)
    emit modified();
  }
}

TelemetrySensorPanel::TelemetrySensorPanel(QWidget *parent, SensorData & sensor, ModelData & model, GeneralSettings & generalSettings, Firmware * firmware):
  ModelPanel(parent, model, generalSettings, firmware),
  ui(new Ui::TelemetrySensor),
  sensor(sensor),
  lock(false)
{
  ui->setupUi(this);
  ui->id->setField(sensor.id);
  ui->instance->setField(sensor.instance);
  ui->ratio->setField(sensor.ratio);
  ui->offset->setField(sensor.offset);
  ui->autoOffset->setField(sensor.autoOffset);
  ui->filter->setField(sensor.filter);
  ui->logs->setField(sensor.logs);
  ui->persistent->setField(sensor.persistent);
  ui->onlyPositive->setField(sensor.onlyPositive);
  ui->gpsSensor->setField(sensor.gps);
  ui->altSensor->setField(sensor.alt);
  ui->ampsSensor->setField(sensor.amps);
  ui->cellsSensor->setField(sensor.source);
  ui->cellsIndex->addItem(tr("Lowest"), SensorData::TELEM_CELL_INDEX_LOWEST);
  for (int i=1; i<=6; i++)
    ui->cellsIndex->addItem(tr("Cell %1").arg(i), i);
  ui->cellsIndex->addItem(tr("Highest"), SensorData::TELEM_CELL_INDEX_HIGHEST);
  ui->cellsIndex->addItem(tr("Delta"), SensorData::TELEM_CELL_INDEX_DELTA);
  ui->cellsIndex->setField(sensor.index);
  ui->source1->setField(sensor.sources[0]);
  ui->source2->setField(sensor.sources[1]);
  ui->source3->setField(sensor.sources[2]);
  ui->source4->setField(sensor.sources[3]);
  update();
}

TelemetrySensorPanel::~TelemetrySensorPanel()
{
  delete ui;
}

void TelemetrySensorPanel::update()
{
  bool isConfigurable = false;
  bool gpsFieldsDisplayed = false;
  bool cellsFieldsDisplayed = false;
  bool consFieldsDisplayed = false;
  bool ratioFieldsDisplayed = false;
  bool sources12FieldsDisplayed = false;
  bool sources34FieldsDisplayed = false;

  lock = true;
  ui->name->setText(sensor.label);
  ui->type->setCurrentIndex(sensor.type);
  ui->unit->setCurrentIndex(sensor.unit);
  ui->prec->setValue(sensor.prec);

  if (sensor.type == SensorData::TELEM_TYPE_CALCULATED) {
    sensor.updateUnit();
    ui->idLabel->hide();
    ui->id->hide();
    ui->instanceLabel->hide();
    ui->instance->hide();
    ui->formula->show();
    ui->formula->setCurrentIndex(sensor.formula);
    isConfigurable = (sensor.formula < SensorData::TELEM_FORMULA_CELL);
    gpsFieldsDisplayed = (sensor.formula == SensorData::TELEM_FORMULA_DIST);
    cellsFieldsDisplayed = (sensor.formula == SensorData::TELEM_FORMULA_CELL);
    consFieldsDisplayed = (sensor.formula == SensorData::TELEM_FORMULA_CONSUMPTION);
    sources12FieldsDisplayed = (sensor.formula <= SensorData::TELEM_FORMULA_MULTIPLY);
    sources34FieldsDisplayed = (sensor.formula < SensorData::TELEM_FORMULA_MULTIPLY);
    updateSourcesComboBox(ui->source1, true);
    updateSourcesComboBox(ui->source2, true);
    updateSourcesComboBox(ui->source3, true);
    updateSourcesComboBox(ui->source4, true);
    updateSourcesComboBox(ui->gpsSensor, false);
    updateSourcesComboBox(ui->altSensor, false);
    updateSourcesComboBox(ui->ampsSensor, false);
    updateSourcesComboBox(ui->cellsSensor, false);
  }
  else {
    ui->idLabel->show();
    ui->id->show();
    ui->instanceLabel->show();
    ui->instance->show();
    ui->formula->hide();
    isConfigurable = sensor.unit < SensorData::UNIT_FIRST_VIRTUAL;
    ratioFieldsDisplayed = (sensor.unit < SensorData::UNIT_FIRST_VIRTUAL);
    ui->offset->setMaximum((sensor.prec > 0 ? sensor.prec == 2 ? 30000 : 3000 : 300));
    ui->offset->setMinimum((sensor.prec > 0 ? sensor.prec == 2 ? -30000 : -3000 : -300));

    if (sensor.unit == SensorData::UNIT_RPMS) {
      ui->offset->setDecimals(0);
      ui->ratio->setDecimals(0);
      ui->autoOffset->hide();
      ui->ratio->setMinimum(1);
      ui->offset->setMinimum(1);
    }
    else {
      ui->offset->setDecimals(sensor.prec);
      ui->ratio->setDecimals(1);
    }
  }

  ui->ratioLabel->setVisible(ratioFieldsDisplayed && sensor.unit != SensorData::UNIT_RPMS);
  ui->bladesLabel->setVisible(sensor.unit == SensorData::UNIT_RPMS);
  ui->ratio->setVisible(ratioFieldsDisplayed);
  ui->offsetLabel->setVisible(ratioFieldsDisplayed && sensor.unit != SensorData::UNIT_RPMS);
  ui->multiplierLabel->setVisible(sensor.unit == SensorData::UNIT_RPMS);
  ui->offset->setVisible(ratioFieldsDisplayed);
  ui->precLabel->setVisible(isConfigurable);
  ui->prec->setVisible(isConfigurable && sensor.unit != SensorData::UNIT_FAHRENHEIT);
  ui->unit->setVisible((sensor.type == SensorData::TELEM_TYPE_CALCULATED && (sensor.formula == SensorData::TELEM_FORMULA_DIST)) || isConfigurable);
  ui->gpsSensorLabel->setVisible(gpsFieldsDisplayed);
  ui->gpsSensor->setVisible(gpsFieldsDisplayed);
  ui->altSensorLabel->setVisible(gpsFieldsDisplayed);
  ui->altSensor->setVisible(gpsFieldsDisplayed);
  ui->ampsSensorLabel->setVisible(consFieldsDisplayed);
  ui->ampsSensor->setVisible(consFieldsDisplayed);
  ui->cellsSensorLabel->setVisible(cellsFieldsDisplayed);
  ui->cellsSensor->setVisible(cellsFieldsDisplayed);
  ui->cellsIndex->setVisible(cellsFieldsDisplayed);
  ui->source1->setVisible(sources12FieldsDisplayed);
  ui->source2->setVisible(sources12FieldsDisplayed);
  ui->source3->setVisible(sources34FieldsDisplayed);
  ui->source4->setVisible(sources34FieldsDisplayed);
  ui->autoOffset->setVisible(sensor.unit != SensorData::UNIT_RPMS && isConfigurable);
  ui->filter->setVisible(isConfigurable);
  ui->persistent->setVisible((sensor.type == SensorData::TELEM_TYPE_CALCULATED && sensor.formula == SensorData::TELEM_FORMULA_CONSUMPTION) || isConfigurable);

  lock = false;
}

void populateTelemetrySourcesComboBox(AutoComboBox * cb, const ModelData * model, bool negative)
{
  cb->clear();
  if (negative) {
    for (int i=-C9X_MAX_SENSORS; i<0; ++i) {
      if (model->sensorData[-i-1].isAvailable())
        cb->addItem(QObject::tr("-%1").arg(model->sensorData[-i-1].label), i);
    }
  }
  cb->addItem("---", 0);
  for (int i=1; i<=C9X_MAX_SENSORS; ++i) {
    if (model->sensorData[i-1].isAvailable())
      cb->addItem(model->sensorData[i-1].label, i);
  }
}

void TelemetrySensorPanel::updateSourcesComboBox(AutoComboBox * cb, bool negative)
{
  populateTelemetrySourcesComboBox(cb, model, negative);
}

void TelemetrySensorPanel::on_name_editingFinished()
{
  if (!lock) {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    strcpy(sensor.label, ui->name->text().toAscii());
#else
    strcpy(sensor.label, ui->name->text().toLatin1());
#endif
    emit nameModified();
    emit modified();
  }
}

void TelemetrySensorPanel::on_type_currentIndexChanged(int index)
{
  if (!lock) {
    sensor.type = index;
    update();
    emit modified();
  }
}

void TelemetrySensorPanel::on_formula_currentIndexChanged(int index)
{
  if (!lock) {
    sensor.formula = index;
    if (sensor.formula == SensorData::TELEM_FORMULA_CELL) {
      sensor.prec = 2;
      sensor.unit = SensorData::UNIT_VOLTS;
    }
    update();
    emit modified();
  }
}

void TelemetrySensorPanel::on_unit_currentIndexChanged(int index)
{
  if (!lock) {
    sensor.unit = index;
    update();
    emit modified();
  }
}

void TelemetrySensorPanel::on_prec_valueChanged()
{
  if (!lock) {
    sensor.prec = ui->prec->value();
    update();
    emit modified();
  }
}

/******************************************************/

TelemetryPanel::TelemetryPanel(QWidget *parent, ModelData & model, GeneralSettings & generalSettings, Firmware * firmware):
  ModelPanel(parent, model, generalSettings, firmware),
  ui(new Ui::Telemetry)
{
  ui->setupUi(this);

  if (firmware->getCapability(NoTelemetryProtocol)) {
    model.frsky.usrProto = 1;
  }

  if (IS_ARM(firmware->getBoard())) {
    ui->A1GB->hide();
    ui->A2GB->hide();
    for (int i=0; i<C9X_MAX_SENSORS; ++i) {
      TelemetrySensorPanel * panel = new TelemetrySensorPanel(this, model.sensorData[i], model, generalSettings, firmware);
      ui->sensorsLayout->addWidget(panel);
      sensorPanels[i] = panel;
      connect(panel, SIGNAL(nameModified()), this, SLOT(update()));
    }
  }
  else {
    ui->sensorsGB->hide();
    analogs[0] = new TelemetryAnalog(this, model.frsky.channels[0], model, generalSettings, firmware);
    ui->A1Layout->addWidget(analogs[0]);
    connect(analogs[0], SIGNAL(modified()), this, SLOT(onAnalogModified()));
    analogs[1] = new TelemetryAnalog(this, model.frsky.channels[1], model, generalSettings, firmware);
    ui->A2Layout->addWidget(analogs[1]);
    connect(analogs[1], SIGNAL(modified()), this, SLOT(onAnalogModified()));
  }

  if (IS_TARANIS(firmware->getBoard())) {
    ui->voltsSource->setField(model.frsky.voltsSource);
    ui->altitudeSource->setField(model.frsky.altitudeSource);
    ui->varioSource->setField(model.frsky.varioSource);
    ui->varioCenterSilent->setField(model.frsky.varioCenterSilent);
  }
  else {
    ui->topbarGB->hide();
  }

  for (int i=0; i<firmware->getCapability(TelemetryCustomScreens); i++) {
    TelemetryCustomScreen * tab = new TelemetryCustomScreen(this, model, model.frsky.screens[i], generalSettings, firmware);
    ui->customScreens->addTab(tab, tr("Telemetry screen %1").arg(i+1));
    telemetryCustomScreens[i] = tab;
  }

  disableMouseScrolling();

  setup();
}

TelemetryPanel::~TelemetryPanel()
{
  delete ui;
}

void TelemetryPanel::update()
{
  if (IS_TARANIS(firmware->getBoard())) {
    if (model->moduleData[0].protocol == OFF && model->moduleData[1].protocol == PPM) {
      ui->telemetryProtocol->setEnabled(true);
    }
    else {
      ui->telemetryProtocol->setEnabled(false);
      ui->telemetryProtocol->setCurrentIndex(0);
    }

    populateTelemetrySourcesComboBox(ui->voltsSource, model, false);
    populateTelemetrySourcesComboBox(ui->altitudeSource, model, false);
    populateTelemetrySourcesComboBox(ui->varioSource, model, false);
  }

  if (IS_ARM(firmware->getBoard())) {
    for (int i=0; i<C9X_MAX_SENSORS; ++i) {
      sensorPanels[i]->update();
    }
    for (int i=0; i<firmware->getCapability(TelemetryCustomScreens); i++) {
      telemetryCustomScreens[i]->update();
    }
  }
}

void TelemetryPanel::setup()
{
    QString firmware_id = g.profile[g.id()].fwType();

    lock = true;

    if (IS_ARM(firmware->getBoard())) {
      ui->telemetryProtocol->addItem(tr("FrSky S.PORT"), 0);
      ui->telemetryProtocol->addItem(tr("FrSky D"), 1);
      if (IS_9XRPRO(firmware->getBoard())) {
        ui->telemetryProtocol->addItem(tr("FrSky D (cable)"), 2);
      }
      ui->telemetryProtocol->setCurrentIndex(model->telemetryProtocol);
      ui->ignoreSensorIds->setField(model->frsky.ignoreSensorIds);
    }
    else {
      ui->telemetryProtocolLabel->hide();
      ui->telemetryProtocol->hide();
      ui->ignoreSensorIds->hide();
    }

    ui->rssiAlarm1SB->setValue(model->frsky.rssiAlarms[0].value);
    ui->rssiAlarm2SB->setValue(model->frsky.rssiAlarms[1].value);
    if (!IS_TARANIS(firmware->getBoard())) {
      ui->rssiAlarm1CB->setCurrentIndex(model->frsky.rssiAlarms[0].level);
      ui->rssiAlarm2CB->setCurrentIndex(model->frsky.rssiAlarms[1].level);
    }
    else {
      ui->rssiAlarm1CB->hide();
      ui->rssiAlarm2CB->hide();
      ui->rssiAlarm1Label->setText(tr("Low Alarm"));
      ui->rssiAlarm2Label->setText(tr("Critical Alarm"));
    }

    /*if (IS_ARM(firmware->getBoard())) {
      for (int i=0; i<C9X_MAX_SENSORS; ++i) {
        TelemetrySensorPanel * panel = new TelemetrySensorPanel(this, model->, model, generalSettings, firmware);
        ui->sensorsLayout->addWidget(panel);
        sensorPanels[i] = panel;
      }
    }*/

    int varioCap = firmware->getCapability(HasVario);
    if (!varioCap) {
      ui->varioLimitMax_DSB->hide();
      ui->varioLimitMin_DSB->hide();
      ui->varioLimitCenterMin_DSB->hide();
      ui->varioLimitCenterMax_DSB->hide();
      ui->varioLimit_label->hide();
      ui->VarioLabel_1->hide();
      ui->VarioLabel_2->hide();
      ui->VarioLabel_3->hide();
      ui->VarioLabel_4->hide();
      ui->varioSource->hide();
      ui->varioSource_label->hide();
    }
    else {
      if (!firmware->getCapability(HasVarioSink)) {
        ui->varioLimitMin_DSB->hide();
        ui->varioLimitCenterMin_DSB->hide();
        ui->VarioLabel_1->hide();
        ui->VarioLabel_2->hide();
      }
      ui->varioLimitMin_DSB->setValue(model->frsky.varioMin-10);
      ui->varioLimitMax_DSB->setValue(model->frsky.varioMax+10);
      ui->varioLimitCenterMax_DSB->setValue((model->frsky.varioCenterMax/10.0)+0.5);
      ui->varioLimitCenterMin_DSB->setValue((model->frsky.varioCenterMin/10.0)-0.5);
    }

    ui->altimetryGB->setVisible(firmware->getCapability(HasVario)),
    ui->frskyProtoCB->setDisabled(firmware->getCapability(NoTelemetryProtocol));

    if (firmware->getCapability(Telemetry) & TM_HASWSHH) {
      ui->frskyProtoCB->addItem(tr("Winged Shadow How High"));
    }
    else {
      ui->frskyProtoCB->addItem(tr("Winged Shadow How High (not supported)"));
    }
    
    ui->variousGB->hide();
    if (!IS_ARM(firmware->getBoard())) {
      if (!(firmware->getCapability(HasFasOffset)) && !(firmware_id.contains("fasoffset"))) {
        ui->fasOffset_label->hide();
        ui->fasOffset_DSB->hide();
      }
      else {
        ui->fasOffset_DSB->setValue(model->frsky.fasOffset/10.0);
        ui->variousGB->show();
      }

      if (!(firmware->getCapability(HasMahPersistent))) {
        ui->mahCount_label->hide();
        ui->mahCount_SB->hide();
        ui->mahCount_ChkB->hide();
      }
      else {
        if (model->frsky.mAhPersistent) {
          ui->mahCount_ChkB->setChecked(true);
          ui->mahCount_SB->setValue(model->frsky.storedMah);
        }
        else {
          ui->mahCount_SB->setDisabled(true);
        }
        ui->variousGB->show();
      }

      ui->frskyProtoCB->setCurrentIndex(model->frsky.usrProto);
      ui->bladesCount->setValue(model->frsky.blades);
      populateVarioSource();
      populateVoltsSource();
      populateCurrentSource();
    }

    lock = false;
}

void TelemetryPanel::populateVarioSource()
{
  AutoComboBox * cb = ui->varioSource;
  cb->setField(model->frsky.varioSource, this);
  cb->addItem(tr("Alti"), TELEMETRY_VARIO_SOURCE_ALTI);
  cb->addItem(tr("Alti+"), TELEMETRY_VARIO_SOURCE_ALTI_PLUS);
  cb->addItem(tr("VSpeed"), TELEMETRY_VARIO_SOURCE_VSPEED);
  cb->addItem(tr("A1"), TELEMETRY_VARIO_SOURCE_A1);
  cb->addItem(tr("A2"), TELEMETRY_VARIO_SOURCE_A2);
}

void TelemetryPanel::populateVoltsSource()
{
  AutoComboBox * cb = ui->frskyVoltCB;
  cb->setField(model->frsky.voltsSource, this);
  cb->addItem(tr("A1"), TELEMETRY_VOLTS_SOURCE_A1);
  cb->addItem(tr("A2"), TELEMETRY_VOLTS_SOURCE_A2);
  if (IS_ARM(firmware->getBoard())) {
    cb->addItem(tr("A3"), TELEMETRY_VOLTS_SOURCE_A3);
    cb->addItem(tr("A4"), TELEMETRY_VOLTS_SOURCE_A4);
  }
  cb->addItem(tr("FAS"), TELEMETRY_VOLTS_SOURCE_FAS);
  cb->addItem(tr("Cells"), TELEMETRY_VOLTS_SOURCE_CELLS);
}

void TelemetryPanel::populateCurrentSource()
{
  AutoComboBox * cb = ui->frskyCurrentCB;
  cb->setField(model->frsky.currentSource, this);
  cb->addItem(tr("---"), TELEMETRY_CURRENT_SOURCE_NONE);
  cb->addItem(tr("A1"), TELEMETRY_CURRENT_SOURCE_A1);
  cb->addItem(tr("A2"), TELEMETRY_CURRENT_SOURCE_A2);
  if (IS_ARM(firmware->getBoard())) {
    cb->addItem(tr("A3"), TELEMETRY_CURRENT_SOURCE_A3);
    cb->addItem(tr("A4"), TELEMETRY_CURRENT_SOURCE_A4);
  }
  cb->addItem(tr("FAS"), TELEMETRY_CURRENT_SOURCE_FAS);
}

void TelemetryPanel::on_telemetryProtocol_currentIndexChanged(int index)
{
  if (!lock) {
    model->telemetryProtocol = index;
    emit modified();
  }
}

void TelemetryPanel::onAnalogModified()
{
  emit modified();
}

void TelemetryPanel::on_bladesCount_editingFinished()
{
  if (!lock) {
    model->frsky.blades = ui->bladesCount->value();
    emit modified();
  }
}

void TelemetryPanel::on_frskyProtoCB_currentIndexChanged(int index)
{
  if (!lock) {
    model->frsky.usrProto = index;
    for (int i=0; i<firmware->getCapability(TelemetryCustomScreens); i++)
      telemetryCustomScreens[i]->update();
    emit modified();
  }
}

void TelemetryPanel::on_rssiAlarm1CB_currentIndexChanged(int index)
{
  model->frsky.rssiAlarms[0].level = index;
  emit modified();
}

void TelemetryPanel::on_rssiAlarm2CB_currentIndexChanged(int index)
{
  model->frsky.rssiAlarms[1].level = index;
  emit modified();
}

void TelemetryPanel::on_rssiAlarm1SB_editingFinished()
{
  model->frsky.rssiAlarms[0].value = ui->rssiAlarm1SB->value();
  emit modified();
}

void TelemetryPanel::on_rssiAlarm2SB_editingFinished()
{
  model->frsky.rssiAlarms[1].value = ui->rssiAlarm2SB->value();
  emit modified();
}

void TelemetryPanel::on_varioLimitMin_DSB_editingFinished()
{
  model->frsky.varioMin = round(ui->varioLimitMin_DSB->value()+10);
  emit modified();
}

void TelemetryPanel::on_varioLimitMax_DSB_editingFinished()
{
  model->frsky.varioMax = round(ui->varioLimitMax_DSB->value()-10);
  emit modified();
}

void TelemetryPanel::on_varioLimitCenterMin_DSB_editingFinished()
{
  if (!lock) {
    if (ui->varioLimitCenterMin_DSB->value()>ui->varioLimitCenterMax_DSB->value()) {
      ui->varioLimitCenterMax_DSB->setValue(ui->varioLimitCenterMin_DSB->value());
    }
    model->frsky.varioCenterMin = round((ui->varioLimitCenterMin_DSB->value()+0.5)*10);
    emit modified();
  }
}

void TelemetryPanel::on_varioLimitCenterMax_DSB_editingFinished()
{
  if (!lock) {
    if (ui->varioLimitCenterMin_DSB->value()>ui->varioLimitCenterMax_DSB->value()) {
      ui->varioLimitCenterMax_DSB->setValue(ui->varioLimitCenterMin_DSB->value());
    }
    model->frsky.varioCenterMax = round((ui->varioLimitCenterMax_DSB->value()-0.5)*10);
    emit modified();
  }
}

void TelemetryPanel::on_fasOffset_DSB_editingFinished()
{
  model->frsky.fasOffset = ui->fasOffset_DSB->value() * 10;
  emit modified();
}

void TelemetryPanel::on_mahCount_SB_editingFinished()
{
  model->frsky.storedMah = ui->mahCount_SB->value();
  emit modified();
}

void TelemetryPanel::on_mahCount_ChkB_toggled(bool checked)
{
  model->frsky.mAhPersistent = checked;
  ui->mahCount_SB->setDisabled(!checked);
  emit modified();
}
