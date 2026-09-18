#ifndef IMAGEEDITDIALOG_H
#define IMAGEEDITDIALOG_H

#include <QByteArray>
#include <QDialog>

class QLineEdit;
class QLabel;

class ImageEditDialog final : public QDialog {
public:
  enum ResultCode { DeleteRequested = QDialog::Accepted + 1 };
  ImageEditDialog(const QByteArray &data, const QString &legend, QWidget *parent = nullptr);
  QByteArray imageData() const { return m_data; }
  QString legend() const;

private:
  bool showImage(const QByteArray &data, bool warn);
  QByteArray m_data;
  QLabel *m_preview;
  QLineEdit *m_legend;
};

#endif
