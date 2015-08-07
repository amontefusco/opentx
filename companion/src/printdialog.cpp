#include "printdialog.h"
#include "ui_printdialog.h"
#include "helpers.h"
#include "helpers_html.h"
#include "eeprominterface.h"
#include "modelprinter.h"
#include <QtGui>
#include <QImage>
#include <QColor>
#include <QPainter>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QPrinter>
#include <QPrintDialog>
#endif

#if !defined WIN32 && defined __GNUC__
#include <unistd.h>
#endif

#define ISIZE 200 // curve image size
#define ISIZEW 400 // curve image size

PrintDialog::PrintDialog(QWidget *parent, Firmware * firmware, GeneralSettings *gg, ModelData *gm, QString filename) :
  QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
  firmware(firmware),
  g_eeGeneral(gg),
  g_model(gm),
  printfilename(filename),
  ui(new Ui::PrintDialog),
  gvars(firmware->getCapability(Gvars)),
  modelPrinter(firmware, gg, gm)
{
  ui->setupUi(this);
  setWindowIcon(CompanionIcon("print.png"));
  te = ui->textEdit;

  setWindowTitle(tr("Setup for: ") + g_model->name);
  te->clear();
  QString modelname=g_model->name;
  if (modelname.isEmpty()) {
    curvefile5=generateProcessUniqueTempFileName("curve5.png");
    curvefile9=generateProcessUniqueTempFileName("curve9.png");  
  }
  else {
    curvefile5=generateProcessUniqueTempFileName(QString("%1-curve5.png").arg(modelname));
    curvefile9=generateProcessUniqueTempFileName(QString("%1-curve9.png").arg(modelname));
  }
  printSetup(); 
  if (gvars) {
    te->append(printFlightModes()+"<br>");
  }
  printInputs();
  printMixes();
  printLimits();
  printCurves();
  printGvars();
  printLogicalSwitches();
  printCustomFunctions();
  printFrSky();
  
  te->scrollToAnchor("1");
  if (!printfilename.isEmpty()) {
    printToFile();
    QTimer::singleShot(0, this, SLOT(autoClose()));
  }
}

void PrintDialog::closeEvent(QCloseEvent *event) 
{
}

PrintDialog::~PrintDialog()
{
  // notice PrintDialog::closeEvent() is not called if user clicks on Close button
  qunlink(curvefile5);
  qunlink(curvefile9);
  delete ui;
}

void PrintDialog::printSetup()
{
    QString str = "<a name=1></a><table border=1 cellspacing=0 cellpadding=3 width=\"100%\">";
    str.append(QString("<tr><td colspan=%1 ><table border=0 width=\"100%\"><tr><td><h1>").arg((firmware->getCapability(FlightModes) && !gvars) ? 2 : 1));
    str.append(g_model->name);
    str.append("&nbsp;(");
    str.append(firmware->getName());
    str.append(")</h1></td><td align=right valign=top NOWRAP><font size=-1>"+tr("printed on: %1").arg(QDateTime::currentDateTime().toString(Qt::SystemLocaleShortDate))+"</font></td></tr></table></td></tr><tr><td><table border=0 cellspacing=0 cellpadding=3>");
    str.append("<tr><td><h2>"+tr("General Model Settings")+"</h2></td></tr>");
    str.append("<tr><td>");
    str.append(fv(tr("Name"), g_model->name));
    str.append(fv(tr("EEprom Size"), QString("%1").arg(firmware->getEepromInterface()->getSize(*g_model))));

    for (int i=0; i<firmware->getCapability(Timers); i++) {
      str.append(fv(tr("Timer%1").arg(i+1), getTimerStr(g_model->timers[i])));  // value, mode, count up/down
    }

    if (firmware->getCapability(NumModules)>1) {
      str.append("<b>"+(IS_TARANIS(firmware->getBoard()) ? tr("Internal Radio System") : tr("Radio System") )+"</b><br>&nbsp;&nbsp;"); //proto, numch, delay,
    }
    str.append(fv(tr("Protocol"), getProtocol(g_model->moduleData[0]))); 
    if (firmware->getCapability(NumModules)>1) {
      str.append("<b>"+(IS_TARANIS(firmware->getBoard()) ? tr("External Radio Module") : tr("Extra Radio System"))+"</b><br>&nbsp;&nbsp;"); //proto, numch, delay,
      str.append(fv(tr("Protocol"), getProtocol(g_model->moduleData[1])));
    }
    if (IS_TARANIS(firmware->getBoard())){
      str.append(fv(tr("Trainer port mode"), getTrainerMode(g_model->trainerMode, g_model->moduleData[2]))); 
    }
    str.append(fv(tr("Throttle Trim"), g_model->thrTrim ? tr("Enabled") : tr("Disabled")));
    str.append(fv(tr("Trim Increment"), getTrimInc(g_model)));
    str.append(fv(tr("Center Beep"), getCenterBeepStr(g_model))); // specify which channels beep
    str.append("</td></tr></table></td>");
    if (!gvars) {
      str.append("<td width=\"380\">");
      str.append(printFlightModes());
      str.append("</td>");
    }
    str.append("</tr></table><br>");
    te->append(str);
}

QString PrintDialog::printFlightModes()
{      
    QString str = "";
    str.append(QString("<table border=1 cellspacing=0 cellpadding=3 width=\"100%\"><tr><td colspan=%1><h2>").arg(!gvars ? 8+firmware->getCapability(RotaryEncoders) : 8+gvars+firmware->getCapability(RotaryEncoders)));
    str.append(tr("Flight modes"));
    str.append("</h2></td></tr><tr><td style=\"border-style:none;\">&nbsp;</td><td colspan=2 align=center><b>");
    str.append(tr("Fades")+"</b></td>");
    str.append("<td colspan=4 align=center><b>"+tr("Trims")+"</b></td>");
    if (gvars) {
      str.append(QString("<td colspan=%1 align=center><b>").arg(gvars)+tr("Gvars")+"</b></td>");
    }
    if (firmware->getCapability(RotaryEncoders)) {
      str.append(QString("<td colspan=%1 align=center><b>").arg(firmware->getCapability(RotaryEncoders))+tr("Rot.Enc.")+"</b></td>");
    }
    str.append("<td rowspan=2 align=\"center\" valign=\"bottom\"><b>"+tr("Switch")+"</b></td></tr><tr><td align=center nowrap><b>"+tr("Flight mode name"));
    str.append("</b></td><td align=center ><b>"+tr("IN")+"</b></td><td align=center ><b>"+tr("OUT")+"</b></td>");
    QString labels[] = { tr("Rud"), tr("Ele"), tr("Thr"), tr("Ail") }; // TODO is elsewhere for sure
    for (int i=0; i<4; i++) {
      GeneralSettings generalSettings = *g_eeGeneral;
      str.append(QString("<td  align=\"center\" nowrap><b>%1</b></td>").arg((QString)labels[CONVERT_MODE(i+1)-1]));
    }
    for (unsigned int i=0; i<gvars; i++) {
      str.append(QString("<td  align=\"center\" nowrap><b>GV%1</b><br>%2</td>").arg(i+1).arg(g_model->gvars_names[i]));
    }
    for (int i=0; i<firmware->getCapability(RotaryEncoders); i++) {
      str.append(QString("<td align=\"center\"><b>RE%1</b></td>").arg((i==0 ? 'A': 'B')));
    }
    str.append("</tr>");
    for (int i=0; i<firmware->getCapability(FlightModes); i++) {
      FlightModeData *pd=&g_model->flightModeData[i];
      str.append("<tr><td valign=\"middle\"><b>"+tr("FM")+QString("%1</b> <font size=+1 face='Courier New' color=green>%2</font></td>").arg(i).arg(pd->name));
      str.append(QString("<td valign=\"middle\" align=\"right\"><font size=+1 face='Courier New' color=green>%1</font></td>").arg((qreal)pd->fadeIn/firmware->getCapability(SlowScale)));
      str.append(QString("<td valign=\"middle\" width=\"30\" align=\"right\"><font size=+1 face='Courier New' color=green>%1</font></td>").arg((qreal)pd->fadeOut/firmware->getCapability(SlowScale)));
      for (int k=0; k<4; k++) {
        //TODO trim values
        if (pd->trimMode[k]==-1) {
          str.append(QString("<td valign=\"middle\" align=\"center\"><font size=+1 face='Courier New' color=grey>")+tr("Off")+QString("</font></td>"));
        } else {
          if (pd->trimRef[k]==i) {
            str.append("<td valign=\"middle\" align=\"center\" ><font size=+1 face='Courier New' color=green>"+QString("%1").arg(pd->trim[k])+QString("</font></td>"));
          } else {
            if (pd->trimMode[k]==0) {
              str.append("<td valign=\"middle\" align=\"center\" ><font size=+1 face='Courier New' color=green>"+tr("FM")+QString("%1").arg(pd->trimRef[k])+QString("</font></td>"));
            } else {
              str.append("<td valign=\"middle\" align=\"center\" ><font size=+1 face='Courier New' color=green>"+tr("FM")+QString("%1+").arg(pd->trimRef[k])+QString("<br>%1").arg(pd->trim[k])+QString("</font></td>"));
            }
          }
        }
      }
      for (unsigned int k=0; k<gvars; k++) {
        if (pd->gvars[k]<=1024) {
          str.append(QString("<td align=\"right\" valign=\"middle\"><font size=+1 face='Courier New' color=green>%1").arg(pd->gvars[k])+"</font></td>");
        }
        else {
          int num = pd->gvars[k] - 1025;
          if (num>=i) num++;
          str.append("<td align=\"right\" valign=\"middle\"><font size=+1 face='Courier New' color=green>"+tr("FM")+QString("%1</font></td>").arg(num));
        }
      }
      for (int k=0; k<firmware->getCapability(RotaryEncoders); k++) {
        if (pd->rotaryEncoders[k]<=1024) {
          str.append(QString("<td align=\"right\" valign=\"middle\"><font size=+1 face='Courier New' color=green>%1").arg(pd->rotaryEncoders[k])+"</font></td>");
        }
        else {
          int num = pd->rotaryEncoders[k] - 1025;
          if (num>=i) num++;
          str.append(QString("<td align=\"right\" valign=\"middle\"><font size=+1 face='Courier New' color=green>")+tr("FM")+QString("%1</font></td>").arg(num));
        }
      }      
      str.append(QString("<td align=center valign=\"middle\"><font size=+1 face='Courier New' color=green>%1</font></td>").arg(pd->swtch.toString()));
      str.append("</tr>");
    }
    str.append("</table>");
    return(str);
}

void PrintDialog::printInputs()
{
    QString str = "<table border=1 cellspacing=0 cellpadding=3 width=\"100%\"><tr><td><h2>";
    str += tr("Inputs");
    str += "</h2></td></tr><tr><td><table border=0 cellspacing=0 cellpadding=3>";
    int ec=0;
    unsigned int lastCHN = 255;
    for(int i=0; i<C9X_MAX_EXPOS; i++) {
      ExpoData *ed=&g_model->expoData[i];
      if(ed->mode==0) continue;
      ec++;

      str += "<tr><td>";
      if(lastCHN!=ed->chn) {
        lastCHN=ed->chn;
        str += addFont("<b>"+modelPrinter.printInputName(ed->chn)+"</b>", "", "+1", "Courier New");
      }
      str += "</td>";
      
      str += "<td>" + addFont(modelPrinter.printInputLine(ed), "green", "+1", "Courier New") + "</td>";
      str += "</tr>";
    }
    str += "</table></td></tr></table><br>";
    if (ec>0) te->append(str);
    // debugHtml(str);
}


void PrintDialog::printMixes()
{
    QString str = "<table border=1 cellspacing=0 cellpadding=3 style=\"page-break-after:always;\" width=\"100%\"><tr><td><h2>";
    str += tr("Mixers");
    str += "</h2></td></tr><tr><td><table border=0 cellspacing=0 cellpadding=3>";

    unsigned int lastCHN = 255;
    for(int i=0; i<firmware->getCapability(Mixes); i++) {
      MixData *md = &g_model->mixData[i];
      if(!md->destCh || md->destCh>(unsigned int)firmware->getCapability(Outputs) ) break;

      str += "<tr><td><b>";
      if(lastCHN!=md->destCh) {
        lastCHN=md->destCh;
        str += addFont(modelPrinter.printMixerName(md->destCh).replace(" ", "&nbsp;"), "", "+1", "Courier New"); 
      } 
      str += "</b></td>";

      str += "<td>" + addFont(modelPrinter.printMixerLine(md), "green", "+1", "Courier New") + "</td>";
      str += "</tr>";
    }
    str += "</table></td></tr></table><br>";
    te->append(str);
    // debugHtml(str);
}

void PrintDialog::printLimits()
{
    QString str = "<table border=1 cellspacing=0 cellpadding=3 width=\"100%\">";
    int numcol;
    numcol=(firmware->getCapability(Outputs)+1)>17 ? 17:firmware->getCapability(Outputs)+1;
    str.append(QString("<tr><td colspan=%1><h2>").arg(numcol)+tr("Limits")+"</h2></td></tr>");
    str.append("<tr><td>&nbsp;</td>");
    if (firmware->getCapability(Outputs)<17) {
      for(int i=0; i<firmware->getCapability(Outputs); i++) {
        str.append(doTC(tr("CH")+QString(" %1").arg(i+1,2,10,QChar('0')),"",true));
      }
      str.append("</tr>");
      if (firmware->getCapability(HasChNames)) {
        str.append("<tr><td><b>"+tr("Name")+"</b></td>");
        for(int i=0; i<firmware->getCapability(Outputs); i++) {
          str.append(doTR(g_model->limitData[i].name, "green"));
        }
      }
      str.append("<tr><td><b>"+tr("Offset")+"</b></td>");
      for(int i=0; i<firmware->getCapability(Outputs); i++) {
        str.append(doTR(g_model->limitData[i].offsetToString(), "green"));
      }
      str.append("</tr>");
      str.append("<tr><td><b>"+tr("Min")+"</b></td>");
      for(int i=0; i<firmware->getCapability(Outputs); i++) {
        str.append(doTR(g_model->limitData[i].minToString(), "green"));
      }
      str.append("</tr>");
      str.append("<tr><td><b>"+tr("Max")+"</b></td>");
      for(int i=0; i<firmware->getCapability(Outputs); i++) {
        str.append(doTR(g_model->limitData[i].maxToString(), "green"));
      }
      str.append("</tr>");
      str.append("<tr><td><b>"+tr("Invert")+"</b></td>");
      for(int i=0; i<firmware->getCapability(Outputs); i++) {
        str.append(doTR(QString(g_model->limitData[i].revert ? tr("INV") : tr("NOR")),"green"));
      }
    } else {
      for(int i=0; i<16; i++) {
        str.append(doTC(tr("CH")+QString(" %1").arg(i+1,2,10,QChar('0')),"",true));
      }
      str.append("</tr>");
      if (firmware->getCapability(HasChNames)) {
        str.append("<tr><td><b>"+tr("Name")+"</b></td>");
        for(int i=0; i<16; i++) {
          str.append(doTR(g_model->limitData[i].name,"green"));
        }
      }
      str.append("<tr><td><b>"+tr("Offset")+"</b></td>");
      for(int i=0; i<16; i++) {
        str.append(doTR(QString::number((qreal)g_model->limitData[i].offset/10, 'f', 1),"green"));
      }
      str.append("</tr>");
      str.append("<tr><td><b>"+tr("Min")+"</b></td>");
      for(int i=0; i<16; i++) {
        str.append(doTR(QString::number((qreal)g_model->limitData[i].min/10),"green"));
      }
      str.append("</tr>");
      str.append("<tr><td><b>"+tr("Max")+"</b></td>");
      for(int i=0; i<16; i++) {
        str.append(doTR(QString::number((qreal)g_model->limitData[i].max/10),"green"));
      }
      str.append("</tr>");
      str.append("<tr><td><b>"+tr("Invert")+"</b></td>");
      for(int i=0; i<16; i++) {
        str.append(doTR(QString(g_model->limitData[i].revert ? tr("INV") : tr("NOR")),"green"));
      }
      str.append("</tr>");
      str.append(QString("<tr><td colspan=%1>&nbsp;").arg(numcol)+"</td></tr>");
      str.append("<tr><td>&nbsp;</td>");
      for(int i=16; i<firmware->getCapability(Outputs); i++) {
        str.append(doTC(tr("CH")+QString(" %1").arg(i+1,2,10,QChar('0')),"",true));
      }
      str.append("</tr>");
      if (firmware->getCapability(HasChNames)) {
        str.append("<tr><td><b>"+tr("Name")+"</b></td>");
        for(int i=16; i<firmware->getCapability(Outputs); i++) {
          str.append(doTR(g_model->limitData[i].name,"green"));
        }
      }
      str.append("<tr><td><b>"+tr("Offset")+"</b></td>");
      for(int i=16; i<firmware->getCapability(Outputs); i++) {
        str.append(doTR(QString::number((qreal)g_model->limitData[i].offset/10, 'f', 1),"green"));
      }
      str.append("</tr>");
      str.append("<tr><td><b>"+tr("Min")+"</b></td>");
      for(int i=16; i<firmware->getCapability(Outputs); i++) {
        str.append(doTR(QString::number((qreal)g_model->limitData[i].min/10),"green"));
      }
      str.append("</tr>");
      str.append("<tr><td><b>"+tr("Max")+"</b></td>");
      for(int i=16; i<firmware->getCapability(Outputs); i++) {
        str.append(doTR(QString::number((qreal)g_model->limitData[i].max/10),"green"));
      }
      str.append("</tr>");
      str.append("<tr><td><b>"+tr("Invert")+"</b></td>");
      for(int i=16; i<firmware->getCapability(Outputs); i++) {
        str.append(doTR(QString(g_model->limitData[i].revert ? tr("INV") : tr("NOR")),"green"));
      }
    }
    str.append("</tr>");
    str.append("</table>");
    str.append("<br>");
    te->append(str);
}

void PrintDialog::printCurves()
{
    int i,r,g,b,c,count,usedcurves=0;
    char buffer[16];
    QPen pen(Qt::black, 2, Qt::SolidLine);
    
    QString str = "<table border=1 cellspacing=0 cellpadding=3 style=\"page-break-before:auto;\" width=\"100%\"><tr><td><h2>";
    str.append(tr("Curves"));
    str.append("</h2></td></tr><tr><td>");
    int numcurves=firmware->getCapability(NumCurves);
    if (numcurves==0) {
      numcurves=16;
    }
    
    QImage qi(ISIZEW+1,ISIZEW+1,QImage::Format_RGB32);
    QPainter painter(&qi);
    painter.setBrush(QBrush("#FFFFFF"));
    painter.setPen(QColor(0,0,0));
    painter.drawRect(0,0,ISIZEW,ISIZEW);
    str.append("<table border=0 cellspacing=0 cellpadding=3 width=\"100%\">"+QString("<tr><td width=\"400\"><img src=\"%1\" border=0></td><td><table border=1 cellspacing=0 cellpadding=3 width=\"100%\">").arg(curvefile5));
    for(i=0; i<numcurves; i++) {
      pen.setColor(colors[usedcurves]);
      painter.setPen(pen);
      colors[usedcurves].getRgb(&r,&g,&b);
      c=r;
      c*=256;
      c+=g;
      c*=256;
      c+=b;
      sprintf(buffer,"%06x",c);
      int curvepoints=g_model->curves[i].count;
      count=0;
      for(int j=0; j<curvepoints; j++) {
        if (g_model->curves[i].points[j].y!=0)
          count++;
      }
      if ((count>0) || (g_model->curves[i].type == CurveData::CURVE_TYPE_CUSTOM)|| (g_model->curves[i].count !=5)) {
        if(usedcurves%2 == 0) str.append("<tr>");
        str.append(QString("<td width=\"70\"><font color=#%1><b>").arg(buffer)+tr("Curve")+QString(" %1</b></font></td>").arg(i+1));
        if(usedcurves%2) str.append("</tr>");
        usedcurves++;
      }
    }
    if (!(usedcurves%2)) str.append("</tr>");
    if (usedcurves!=0) {
      str.append("</table></td></tr><tr><td colspan=2><table border=1 cellspacing=0 cellpadding=3 width=\"100%\">");
      str.append("<tr>");
      str.append(doTC("&nbsp;"));
      str.append(doTC("&nbsp;"));
      int numpoint=0;
      for(i=0; i<numcurves; i++) {
        if (g_model->curves[i].count>numpoint)
          numpoint=g_model->curves[i].count;
      }
      for(i=0; i<numpoint; i++) 
          str.append(doTC(tr("pt %1").arg(i+1), "", true));
      str.append("</tr>");
      for(i=0; i<numcurves; i++) {
        int curvepoints=g_model->curves[i].count;
        count=0;
        for(int j=0; j<curvepoints; j++) {
          if (g_model->curves[i].points[j].y!=0)
            count++;
        }
        if ((count>0) || (g_model->curves[i].type == CurveData::CURVE_TYPE_CUSTOM)) {
          pen.setColor(colors[i]);
          painter.setPen(pen);
          colors[i].getRgb(&r,&g,&b);
          c=r;
          c*=256;
          c+=g;
          c*=256;
          c+=b;
          sprintf(buffer,"%06x",c);
          str.append("<tr>");
          if (g_model->curves[i].type == CurveData::CURVE_TYPE_CUSTOM)
            str.append(QString("<td width=\"70\" rowspan=2 valign=middle><font color=#%1><b>").arg(buffer)+tr("Curve")+QString(" %1</b></font></td><td width=5>Y</td>").arg(i+1));
          else
            str.append(QString("<td width=\"70\"><font color=#%1><b>").arg(buffer)+tr("Curve")+QString(" %1</b></font></td><td width=5>Y</td>").arg(i+1));
          for(int j=0; j<curvepoints; j++) {    
            str.append(doTR(QString::number(g_model->curves[i].points[j].y),"green"));
            if (j>0 && (count!=0 ||(g_model->curves[i].type == CurveData::CURVE_TYPE_CUSTOM))) {
              if (g_model->curves[i].type == CurveData::CURVE_TYPE_CUSTOM)
                painter.drawLine(ISIZEW/2+(ISIZEW*g_model->curves[i].points[j-1].x)/200,ISIZEW/2-(ISIZEW*g_model->curves[i].points[j-1].y)/200,ISIZEW/2+(ISIZEW*g_model->curves[i].points[j].x)/200,ISIZEW/2-(ISIZEW*g_model->curves[i].points[j].y)/200);
              else
                painter.drawLine(ISIZEW*(j-1)/(curvepoints-1),ISIZEW/2-(ISIZEW*g_model->curves[i].points[j-1].y)/200,ISIZEW*(j)/(curvepoints-1),ISIZEW/2-(ISIZEW*g_model->curves[i].points[j].y)/200);
            }
          }
          for(int j=curvepoints; j<numpoint; j++) {
            str.append(doTR("","green"));
          }     
          str.append("</tr>");
          if (g_model->curves[i].type == CurveData::CURVE_TYPE_CUSTOM) {
            str.append("<tr><td width=5>X</td>");
            for(int j=0; j<curvepoints; j++) {    
              str.append(doTR(QString::number(g_model->curves[i].points[j].x),"green"));
            }
            for(int j=curvepoints; j<numpoint; j++) {
              str.append(doTR("","green"));
            }     
            str.append("</tr>");  
          }
        }
      }
      str.append("</table></td></tr></table></td></tr></table>");
      str.append("<br>");
      painter.setPen(QColor(0,0,0));
      painter.drawLine(0,ISIZEW/2,ISIZEW,ISIZEW/2);
      painter.drawLine(ISIZEW/2,0,ISIZEW/2,ISIZEW);
      for(i=0; i<21; i++) {
        painter.drawLine(ISIZEW/2-5,(ISIZEW*i)/(20),ISIZEW/2+5,(ISIZEW*i)/(20));
        painter.drawLine((ISIZEW*i)/(20),ISIZEW/2-5,(ISIZEW*i)/(20),ISIZEW/2+5);
      }
      qi.save(curvefile5, "png",100); 
      te->append(str);
    }
}

void PrintDialog::printLogicalSwitches()
{
  bool haveOutput = false;
  QString str = "<table border=1 cellspacing=0 cellpadding=3 width=\"100%\">";
  str += "<tr><td><h2>" + tr("Logical Switches") + "</h2></td></tr>";
  str += "<tr><td><table border=0 cellspacing=0 cellpadding=3>";
  for (int i=0; i<firmware->getCapability(LogicalSwitches); i++) {
    QString sw = modelPrinter.printLogicalSwitchLine(i);
    if (!sw.isEmpty()) {
      str += "<tr>";
      str += "<td width=\"60\"><b>"+ tr("L") + QString("%1</b></td>").arg(i+1);
      str += "<td align=left>" + addFont(sw, "green") + "</td>";
      str += "</tr>";
      haveOutput = true;
    }
  }
  str += "</table></td></tr></table>";
  str += "<br>";
  if (haveOutput) te->append(str);
}

void PrintDialog::printGvars()
{
  if (!firmware->getCapability(GvarsFlightModes) && gvars) {
    QString str = "<table border=1 cellspacing=0 cellpadding=3 width=\"100%\">";
    str.append("<tr><td><h2>"+tr("Global Variables")+"</h2></td></tr>");
    str.append("<tr><td><table border=1 cellspacing=0 cellpadding=3 width=100>");
    FlightModeData *pd=&g_model->flightModeData[0];
    int width = 100/gvars;
    str.append("<tr>");
    for (unsigned int i=0; i<gvars; i++) {
      str.append(QString("<td width=\"%1%\" align=\"center\"><b>").arg(width)+tr("GV")+QString("%1</b></td>").arg(i+1));
    }
    str.append("</tr>");
    str.append("<tr>");
    for (unsigned int i=0; i<gvars; i++) {
      str.append(QString("<td width=\"%1%\" align=\"center\"><font color=green>").arg(width)+QString("%1</font></td>").arg(pd->gvars[i]));
    }
    str.append("</tr>");
    str.append("</table></td></tr></table>");
    str.append("<br>");
    te->append(str);
  }
}

void PrintDialog::printCustomFunctions()
{
  bool haveOutput = false;
  QString str = "<table border=1 cellspacing=0 cellpadding=3 width=\"100%\">";
  str += "<tr><td><h2>" + tr("Special Functions") + "</h2></td></tr>";
  str += "<tr><td><table border=0 cellspacing=0 cellpadding=3>";
  for (int i=0; i<firmware->getCapability(CustomFunctions); i++) {
    QString cf = modelPrinter.printCustomFunctionLine(i);
    if (!cf.isEmpty()) {
      str += "<tr>";
      str += "<td width=\"60\"><b>"+ tr("SF") + QString("%1</b></td>").arg(i+1);
      str += "<td align=left>" + addFont(cf, "green") + "</td>";
      str += "</tr>";
      haveOutput = true;
    }
  }
  str += "</table></td></tr></table>";
  str += "<br>";
  if (haveOutput) te->append(str);
}

void PrintDialog::printFrSky()
{
  int tc=0;
  QString str = "<table border=1 cellspacing=0 cellpadding=3 width=\"100%\">";
  int analog=2;
  if (IS_ARM(firmware->getBoard())) {
    analog=4;
  }

  if (IS_TARANIS(GetEepromInterface()->getBoard())) {
    str.append("<tr><td colspan=3><h2>"+tr("Telemetry Settings")+"</h2></td></tr>");
    str.append("<tr>");
    str.append(doTC(tr("Analog"),"", true));
    str.append(doTC(tr("Range"),"", true));
    str.append(doTC(tr("Offset"),"", true));
    str.append("</tr>");

    FrSkyData *fd=&g_model->frsky;
    for (int i=0; i<analog; i++) {
      if (fd->channels[i].ratio!=0) {
        tc++;
        float ratio=(fd->channels[i].ratio/(fd->channels[i].type==0 ?10.0:1));
        QString unit = " " + getFrSkyUnits(fd->channels[i].type);
        str.append("<tr>");
        str.append(doTL(tr("A%1").arg(i+1), "", true));
        str.append(doTC(QString::number(ratio,10,(fd->channels[i].type==0 ? 1:0))+unit, "green"));
        str.append(doTC(QString::number((fd->channels[i].offset*ratio)/255,10,(fd->channels[i].type==0 ? 1:0))+unit, "green"));
        str.append("</tr>");
      }
    }

    str.append("<tr><td colspan=3 align=\"Left\" height=\"4px\"></td></tr>");
    str.append("<tr>");
    str.append(doTC(tr("Alarms"),"", true));
    str.append(doTC(tr("Low Alarm"),"", true));
    str.append(doTC(tr("Critical Alarm"),"", true));
    str.append("</tr>");
    str.append("<tr>");
    str.append(doTL(tr("RSSI"),"", true));
    str.append(doTC(QString::number(fd->rssiAlarms[0].value,10),"green"));
    str.append(doTC(QString::number(fd->rssiAlarms[1].value,10),"green"));
    str.append("</tr>");
    for (int i=0; i<2; i++) {
      if (fd->channels[i].ratio!=0) {
        float ratio=(fd->channels[i].ratio/(fd->channels[i].type==0 ?10.0:1));
        QString unit = " " + getFrSkyUnits(fd->channels[i].type);
        str.append("<tr>");
        str.append(doTL(tr("A%1").arg(i+1), "", true));
        str.append(doTC(QString::number(ratio*(fd->channels[i].alarms[0].value/255.0+fd->channels[i].offset/255.0),10,(fd->channels[i].type==0 ? 1:0))+unit, "green"));
        str.append(doTC(QString::number(ratio*(fd->channels[i].alarms[1].value/255.0+fd->channels[i].offset/255.0),10,(fd->channels[i].type==0 ? 1:0))+unit, "green"));
        str.append("</tr>");
      }
    }

    //TODO preferrably in new tables
    str.append("<tr><td colspan=3 align=\"Left\" height=\"4px\"></td></tr>");
    //str.append("<tr><td align=\"Left\"><b>"+tr("Frsky serial protocol")+"</b></td><td colspan=2 align=\"left\">"+getFrSkyProtocol(fd->usrProto)+"</td></tr>");
    //str.append("<tr><td align=\"Left\"><b>"+tr("Units system")+"</b></td><td colspan=2 align=\"left\">"+getFrSkyMeasure(fd->imperial)+"</td></tr>");
    str.append("<tr><td align=\"Left\"><b>"+tr("Blades")+"</b></td><td colspan=2 align=\"left\">"+QString("%1").arg(fd->blades)+"</td></tr>");
    str.append("<tr><td align=\"Left\" height=\"4px\"></td></tr></table>");
  }
  else {  //other boards
    str.append("<tr><td colspan=10><h2>"+tr("Telemetry Settings")+"</h2></td></tr>");
    str.append("<tr><td colspan=4 align=\"center\">&nbsp;</td><td colspan=3 align=\"center\"><b>"+tr("Alarm 1")+"</b></td><td colspan=3 align=\"center\"><b>"+tr("Alarm 2")+"</b></td></tr>");
    str.append("<tr><td align=\"center\"><b>"+tr("Analog")+"</b></td><td align=\"center\"><b>"+tr("Unit")+"</b></td><td align=\"center\"><b>"+tr("Scale")+"</b></td><td align=\"center\"><b>"+tr("Offset")+"</b></td>");
    str.append("<td width=\"40\" align=\"center\"><b>"+tr("Type")+"</b></td><td width=\"40\" align=\"center\"><b>"+tr("Condition")+"</b></td><td width=\"40\" align=\"center\"><b>"+tr("Value")+"</b></td>");
    str.append("<td width=\"40\" align=\"center\"><b>"+tr("Type")+"</b></td><td width=\"40\" align=\"center\"><b>"+tr("Condition")+"</b></td><td width=\"40\" align=\"center\"><b>"+tr("Value")+"</b></td></tr>");
    FrSkyData *fd=&g_model->frsky;
    for (int i=0; i<analog; i++) {
      if (fd->channels[i].ratio!=0) {
        tc++;
        float ratio=(fd->channels[i].ratio/(fd->channels[i].type==0 ?10.0:1));
        str.append("<tr><td align=\"center\"><b>"+tr("A%1").arg(i+1)+"</b></td><td align=\"center\"><font color=green>"+getFrSkyUnits(fd->channels[i].type)+"</font></td><td align=\"center\"><font color=green>"+QString::number(ratio,10,(fd->channels[i].type==0 ? 1:0))+"</font></td><td align=\"center\"><font color=green>"+QString::number((fd->channels[i].offset*ratio)/255,10,(fd->channels[i].type==0 ? 1:0))+"</font></td>");
        str.append("<td width=\"40\" align=\"center\"><font color=green>"+getFrSkyAlarmType(fd->channels[i].alarms[0].level)+"</font></td>");
        str.append("<td width=\"40\" align=\"center\"><font color=green>");
        str.append((fd->channels[i].alarms[0].greater==1) ? "&gt;" : "&lt;");
        str.append("</font></td><td width=\"40\" align=\"center\"><font color=green>"+QString::number(ratio*(fd->channels[i].alarms[0].value/255.0+fd->channels[i].offset/255.0),10,(fd->channels[i].type==0 ? 1:0))+"</font></td>");
        str.append("<td width=\"40\" align=\"center\"><font color=green>"+getFrSkyAlarmType(fd->channels[i].alarms[1].level)+"</font></td>");
        str.append("<td width=\"40\" align=\"center\"><font color=green>");
        str.append((fd->channels[i].alarms[1].greater==1) ? "&gt;" : "&lt;");
        str.append("</font></td><td width=\"40\" align=\"center\"><font color=green>"+QString::number(ratio*(fd->channels[i].alarms[1].value/255.0+fd->channels[i].offset/255.0),10,(fd->channels[i].type==0 ? 1:0))+"</font></td></tr>");
      }
    }
    str.append("<tr><td colspan=10 align=\"Left\" height=\"4px\"></td></tr>");
    str.append("<tr><td colspan=4 align=\"center\" rowspan=2>&nbsp;</td><td colspan=3 align=\"center\"><b>"+tr("Alarm 1")+"</b></td><td colspan=3 align=\"center\"><b>"+tr("Alarm 2")+"</b></td></tr>");
    str.append("<tr><td width=\"40\" align=\"center\"><b>"+tr("Type")+"</b></td><td width=\"40\" align=\"center\"><b>"+tr("Condition")+"</b></td><td width=\"40\" align=\"center\"><b>"+tr("Value")+"</b></td>");
    str.append("<td width=\"40\" align=\"center\"><b>"+tr("Type")+"</b></td><td width=\"40\" align=\"center\"><b>"+tr("Condition")+"</b></td><td width=\"40\" align=\"center\"><b>"+tr("Value")+"</b></td></tr>");
    str.append("<tr><td align=\"Left\" colspan=4><b>"+tr("RSSI Alarm")+"</b></td>");
    str.append("<td width=\"40\" align=\"center\"><b>"+getFrSkyAlarmType(fd->rssiAlarms[0].level)+"</b></td><td width=\"40\" align=\"center\"><b>&lt;</b></td><td width=\"40\" align=\"center\"><b>"+QString::number(fd->rssiAlarms[0].value,10)+"</b></td>");
    str.append("<td width=\"40\" align=\"center\"><b>"+getFrSkyAlarmType(fd->rssiAlarms[1].level)+"</b></td><td width=\"40\" align=\"center\"><b>&lt;</b></td><td width=\"40\" align=\"center\"><b>"+QString::number(fd->rssiAlarms[1].value,10)+"</b></td></tr>");
    str.append("<tr><td colspan=10 align=\"Left\" height=\"4px\"></td></tr>");
    str.append("<tr><td colspan=2 align=\"Left\"><b>"+tr("Frsky serial protocol")+"</b></td><td colspan=8 align=\"left\">"+getFrSkyProtocol(fd->usrProto)+"</td></tr>");
    str.append("<tr><td colspan=2 align=\"Left\"><b>"+tr("Blades")+"</b></td><td colspan=8 align=\"left\">"+QString("%1").arg(fd->blades)+"</td></tr>");
    str.append("<tr><td colspan=10 align=\"Left\" height=\"4px\"></td></tr></table>");
  }
#if 0
  if (firmware->getCapability(TelemetryBars) || (firmware->getCapability(TelemetryCSFields))) {
    int cols=firmware->getCapability(TelemetryColsCSFields);
    if (cols==0) cols=2;
    for (int j=0; j<firmware->getCapability(TelemetryCSFields)/(4*cols); j++ ) {
      if (fd->screens[j].type==0) {
        if (cols==2) {
          str.append("<table border=1 cellspacing=0 cellpadding=3 width=\"100%\"><tr><td colspan=3 align=\"Left\"><b>"+tr("Custom Telemetry View")+"</b></td></tr>");
        } else {
          str.append("<table border=1 cellspacing=0 cellpadding=3 width=\"100%\"><tr><td colspan=5 align=\"Left\"><b>"+tr("Custom Telemetry View")+"</b></td></tr>");
        }
        for (int r=0; r<4; r++) {
          str.append("<tr>");
          for (int c=0; c<cols; c++) {
            if (fd->screens[j].body.lines[r].source[c]!=0)
              tc++;
            if (cols==2) {
              str.append("<td  align=\"Center\" width=\"45%\"><b>"+getFrSkySrc(fd->screens[j].body.lines[r].source[c])+"</b></td>");
            } else {
              str.append("<td  align=\"Center\" width=\"30%\"><b>"+getFrSkySrc(fd->screens[j].body.lines[r].source[c])+"</b></td>");
            }
            if (c<(cols-1)) {
              if (cols==2) {
                str.append("<td  align=\"Center\" width=\"10%\"><b>&nbsp;</b></td>");
              } else {
                str.append("<td  align=\"Center\" width=\"5%\"><b>&nbsp;</b></td>");
              }
            }
          }
          str.append("</tr>");
        }
        str.append("</table>");
      } else {
        str.append("<table border=1 cellspacing=0 cellpadding=3 width=\"100%\"><tr><td colspan=4 align=\"Left\"><b>"+tr("Telemetry Bars")+"</b></td></tr>");
        str.append("<tr><td  align=\"Center\"><b>"+tr("Bar Number")+"</b></td><td  align=\"Center\"><b>"+tr("Source")+"</b></td><td  align=\"Center\"><b>"+tr("Min")+"</b></td><td  align=\"Center\"><b>"+tr("Max")+"</b></td></tr>");
        for (int i=0; i<4; i++) {
          if (fd->screens[j].body.bars[i].source!=0) 
            tc++;
          // TODO str.append("<tr><td  align=\"Center\"><b>"+QString::number(i+1,10)+"</b></td><td  align=\"Center\"><b>"+getFrSkySrc(fd->screens[j].body.bars[i].source)+"</b></td><td  align=\"Right\"><b>"+(fd->screens[j].body.bars[i].source>0 ? QString::number(getBarValue(fd->screens[j].body.bars[i].source, fd->screens[j].body.bars[i].barMin,fd)):"----")+"</b></td><td  align=\"Right\"><b>"+(fd->screens[j].body.bars[i].source>0 ? QString::number(getBarValue(fd->screens[j].body.bars[i].source,(255-fd->screens[j].body.bars[i].barMax),fd)) :"----")+"</b></td></tr>");
        }
        str.append("</table>");
      }
    }
  }
#endif
  if (tc>0)
      te->append(str);    
}

void PrintDialog::on_printButton_clicked()
{
    QPrinter printer;
    printer.setPageMargins(10.0,10.0,10.0,10.0,printer.Millimeter);
    QPrintDialog *dialog = new QPrintDialog(&printer, this);
    dialog->setWindowTitle(tr("Print Document"));
    if (dialog->exec() != QDialog::Accepted)
      return;
    te->print(&printer);
}

void PrintDialog::on_printFileButton_clicked()
{
    QString fn = QFileDialog::getSaveFileName(this,tr("Select PDF output file"),QString(),tr("ODF files (*.odt);;PDF Files(*.pdf);;HTML-Files (*.htm *.html);;All Files (*)")); 
    if (fn.isEmpty())
      return;
    if (! (fn.endsWith(".odt", Qt::CaseInsensitive) || fn.endsWith(".pdf", Qt::CaseInsensitive) || fn.endsWith(".htm", Qt::CaseInsensitive) || fn.endsWith(".html", Qt::CaseInsensitive)) )
      fn += ".pdf"; // default
    if (fn.endsWith(".pdf", Qt::CaseInsensitive)) {
      QPrinter printer;
      printer.setPageMargins(10.0,10.0,10.0,10.0,printer.Millimeter);
      printer.setOutputFormat(QPrinter::PdfFormat);
      printer.setColorMode(QPrinter::Color);
      printer.setOutputFileName(fn);
      te->print(&printer);
    } else {
      QTextDocumentWriter writer(fn);
       writer.write(te->document());      
    }
}

void PrintDialog::printToFile()
{
    if (printfilename.isEmpty())
      return;
    if (! (printfilename.endsWith(".odt", Qt::CaseInsensitive) || printfilename.endsWith(".pdf", Qt::CaseInsensitive) || printfilename.endsWith(".htm", Qt::CaseInsensitive) || printfilename.endsWith(".html", Qt::CaseInsensitive)) )
      printfilename += ".pdf"; // default
    if (printfilename.endsWith(".pdf", Qt::CaseInsensitive)) {
      QPrinter printer;
      printer.setPageMargins(10.0,10.0,10.0,10.0,printer.Millimeter);
      printer.setOutputFormat(QPrinter::PdfFormat);
      printer.setColorMode(QPrinter::Color);
      printer.setOutputFileName(printfilename);
      te->print(&printer);
    } else {
      QTextDocumentWriter writer(printfilename);
       writer.write(te->document());      
    }
}

void PrintDialog::autoClose()
{
  this->close();
}
