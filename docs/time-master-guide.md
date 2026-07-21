# Time Master 接入 LineWeb 推送文档

本文档面向 Time Master（基于 Qt 的本地屏幕时间管理软件）的开发者，说明如何将屏幕使用时间推送到 LineWeb。

## 推送端点

```
POST /api/health/push
Content-Type: application/json
X-Screen-Time-Token: st_your_token
```

**请求体：**

```json
{
  "totalSeconds": 12345,
  "date": "2026-07-14"
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `totalSeconds` | int | 当日累计屏幕使用秒数，范围 0~86400 |
| `date` | string | 日期，格式 `YYYY-MM-DD` |

**成功响应（200）：**

```json
{ "message": "已同步" }
```

**错误响应：**

| 状态码 | 说明 |
|--------|------|
| 400 | `totalSeconds` 为负或超过 86400，或 `date` 格式错误 |
| 401 | Token 无效或已过期 |

## 认证

使用屏幕时间 Token，通过请求头传递：

```
X-Screen-Time-Token: st_xxxxxxxxxxxx
```

Token 由用户在 LineWeb 个人资料页 → 数字健康生成，支持永久 / 7 天 / 30 天有效期。格式：`st_` + 64 位 hex。

## Qt 集成示例

```cpp
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDate>

class LineWebPusher : public QObject {
    Q_OBJECT
public:
    explicit LineWebPusher(QObject* parent = nullptr) : QObject(parent) {
        m_manager = new QNetworkAccessManager(this);
    }

    void setEndpoint(const QString& url) { m_endpoint = url; }
    void setToken(const QString& token) { m_token = token; }

    void push(int totalSeconds, const QDate& date) {
        QJsonObject body;
        body["totalSeconds"] = totalSeconds;
        body["date"] = date.toString("yyyy-MM-dd");

        QNetworkRequest req(QUrl(m_endpoint + "/api/health/push"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        req.setRawHeader("X-Screen-Time-Token", m_token.toUtf8());

        QNetworkReply* reply = m_manager->post(req, QJsonDocument(body).toJson());
        connect(reply, &QNetworkReply::finished, this, [reply]() {
            if (reply->error() == QNetworkReply::NoError) {
                qDebug() << "[LineWeb] 推送成功";
            } else {
                qWarning() << "[LineWeb] 推送失败:" << reply->readAll();
            }
            reply->deleteLater();
        });
    }

private:
    QNetworkAccessManager* m_manager;
    QString m_endpoint;
    QString m_token;
};
```

## 推送策略

- **频率**：每 5~15 分钟推送一次。
- **断线**：不需重试，下次定时推送直接覆盖当日数据。
- **退出**：应用关闭前执行一次最终推送，确保数据不丢。
- **配置**：在 Time Master 设置界面提供 LineWeb Token 输入框和 API 地址配置项。

## Token

- 用户通过 LineWeb 个人资料页 → 数字健康生成。
- 支持永久、7 天、30 天有效期。
- 格式：`st_` + 64 位 hex。
