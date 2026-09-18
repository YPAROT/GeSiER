#include "imageeditdialog.h"

#include <QtWidgets>

class ImagePreviewLabel final : public QLabel {
public:
  using QLabel::QLabel;
  void setImage(const QImage &image) { m_image=image; updatePixmap(); }
protected:
  void resizeEvent(QResizeEvent *event) override { QLabel::resizeEvent(event); updatePixmap(); }
private:
  void updatePixmap() {
    if(m_image.isNull()){clear();setText("Impossible d'afficher cette image.");return;}
    const QSize a=contentsRect().size();
    const QSize target(qMin(a.width(),m_image.width()),qMin(a.height(),m_image.height()));
    setPixmap(QPixmap::fromImage(m_image).scaled(target,Qt::KeepAspectRatio,Qt::SmoothTransformation));
  }
  QImage m_image;
};

ImageEditDialog::ImageEditDialog(const QByteArray &data,const QString &legend,QWidget *parent)
    :QDialog(parent),m_data(data) {
  setWindowTitle("Image du document");resize(760,620);
  auto *preview=new ImagePreviewLabel; m_preview=preview; preview->setObjectName("documentImagePreview");
  preview->setAlignment(Qt::AlignCenter);preview->setMinimumSize(320,240);preview->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);preview->setFrameShape(QFrame::StyledPanel);
  m_legend=new QLineEdit(legend);m_legend->setObjectName("documentImageLegend");
  auto *replace=new QPushButton("Remplacer l'image…");replace->setObjectName("replaceDocumentImage");
  auto *remove=new QPushButton("Supprimer");remove->setObjectName("deleteDocumentImage");
  auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Save)->setText("Enregistrer");buttons->button(QDialogButtonBox::Cancel)->setText("Annuler");
  auto *form=new QFormLayout;form->addRow("Légende",m_legend);auto *actions=new QHBoxLayout;actions->addWidget(replace);actions->addWidget(remove);actions->addStretch();actions->addWidget(buttons);auto *layout=new QVBoxLayout(this);layout->addWidget(preview,1);layout->addLayout(form);layout->addLayout(actions);showImage(m_data,false);
  connect(replace,&QPushButton::clicked,this,[this]{const QString path=QFileDialog::getOpenFileName(this,"Remplacer l'image",{},"Images (*.png *.jpg *.jpeg *.bmp)");if(path.isEmpty())return;QFile file(path);if(!file.open(QIODevice::ReadOnly)){QMessageBox::warning(this,"Image",file.errorString());return;}const QByteArray candidate=file.readAll();if(showImage(candidate,true))m_data=candidate;});
  connect(remove,&QPushButton::clicked,this,[this]{if(QMessageBox::question(this,"Supprimer l'image","Supprimer définitivement cette image du document ?")==QMessageBox::Yes)done(DeleteRequested);});
  connect(buttons,&QDialogButtonBox::accepted,this,[this]{QImage image;if(!image.loadFromData(m_data)){QMessageBox::warning(this,"Image","L'image enregistrée est invalide. Remplacez-la avant d'enregistrer.");return;}accept();});
  connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
}

QString ImageEditDialog::legend() const{return m_legend->text();}
bool ImageEditDialog::showImage(const QByteArray &data,bool warn){QImage image;const bool valid=image.loadFromData(data);static_cast<ImagePreviewLabel*>(m_preview)->setImage(image);if(!valid&&warn)QMessageBox::warning(this,"Image","Le fichier sélectionné n'est pas une image PNG, JPEG ou BMP valide.");return valid;}
