#include "vdf.h"
#include <QFile>
#include <QTextStream>
#include <QSet>
#include <QVector>
#include <cctype>

namespace ProtonSage {

// ── Lexer / Scanner ─────────────────────────────────────────────────

namespace {

enum class TokenKind { Eof, String, LBrace, RBrace };

struct Token {
    TokenKind kind = TokenKind::Eof;
    QString   text;
};

class Scanner {
public:
    explicit Scanner(const QByteArray& data)
        : m_data(QString::fromUtf8(data))
        , m_pos(0)
        , m_line(1)
        , m_col(1)
    {}

    Token nextToken();

private:
    void skipWhitespaceAndComments();
    Token scanQuoted();
    Token scanBare();
    QChar peek() const;
    QChar advance();

    QString m_data;
    int     m_pos = 0;
    int     m_line = 1;
    int     m_col  = 1;
};

QChar Scanner::peek() const
{
    if (m_pos >= m_data.size())
        return QChar();
    return m_data.at(m_pos);
}

QChar Scanner::advance()
{
    QChar ch = m_data.at(m_pos);
    ++m_pos;
    if (ch == QLatin1Char('\n')) {
        ++m_line;
        m_col = 1;
    } else {
        ++m_col;
    }
    return ch;
}

void Scanner::skipWhitespaceAndComments()
{
    while (m_pos < m_data.size()) {
        QChar ch = peek();
        if (ch.isSpace()) {
            advance();
            continue;
        }
        // C++ style // comments (Steam VDF uses these sometimes)
        if (ch == QLatin1Char('/') && m_pos + 1 < m_data.size()
            && m_data.at(m_pos + 1) == QLatin1Char('/')) {
            while (m_pos < m_data.size() && peek() != QLatin1Char('\n'))
                advance();
            continue;
        }
        break;
    }
}

Token Scanner::scanQuoted()
{
    advance(); // consume opening '"'
    QString buf;
    while (m_pos < m_data.size()) {
        QChar ch = advance();
        if (ch == QLatin1Char('"'))
            return Token{TokenKind::String, buf};
        if (ch == QLatin1Char('\\') && m_pos < m_data.size()) {
            QChar next = advance();
            switch (next.unicode()) {
            case 'n': buf += QLatin1Char('\n'); break;
            case 't': buf += QLatin1Char('\t'); break;
            case 'r': buf += QLatin1Char('\r'); break;
            default:  buf += next; break;
            }
            continue;
        }
        buf += ch;
    }
    // Unterminated string — return what we have
    return Token{TokenKind::String, buf};
}

Token Scanner::scanBare()
{
    QString buf;
    while (m_pos < m_data.size()) {
        QChar ch = peek();
        if (ch.isSpace() || ch == QLatin1Char('{') || ch == QLatin1Char('}'))
            break;
        buf += advance();
    }
    return Token{TokenKind::String, buf};
}

Token Scanner::nextToken()
{
    skipWhitespaceAndComments();
    if (m_pos >= m_data.size())
        return Token{TokenKind::Eof, QString()};

    QChar ch = peek();
    switch (ch.unicode()) {
    case '{':
        advance();
        return Token{TokenKind::LBrace, QStringLiteral("{")};
    case '}':
        advance();
        return Token{TokenKind::RBrace, QStringLiteral("}")};
    case '"':
        return scanQuoted();
    default:
        return scanBare();
    }
}

// ── Parser ──────────────────────────────────────────────────────────

class Parser {
public:
    explicit Parser(const QByteArray& data)
        : m_scanner(data)
    {}

    VDFObject parse();

private:
    VDFObject parseObject(bool stopOnRBrace);

    Scanner m_scanner;
};

VDFObject Parser::parse()
{
    return parseObject(false);
}

VDFObject Parser::parseObject(bool stopOnRBrace)
{
    VDFObject obj;
    while (true) {
        Token keyTok = m_scanner.nextToken();

        switch (keyTok.kind) {
        case TokenKind::Eof:
            if (stopOnRBrace)
                return VDFObject(); // error: unexpected EOF inside object
            return obj;
        case TokenKind::RBrace:
            if (stopOnRBrace)
                return obj;
            return VDFObject(); // error: unexpected }
        case TokenKind::LBrace:
            return VDFObject(); // error: unexpected {
        case TokenKind::String:
            break; // continue below
        }

        Token valueTok = m_scanner.nextToken();
        switch (valueTok.kind) {
        case TokenKind::String:
            obj.insert(keyTok.text, QVariant(valueTok.text));
            break;
        case TokenKind::LBrace: {
            VDFObject child = parseObject(true);
            if (child.isEmpty() && !stopOnRBrace) {
                // Could be an error, but still store empty object
            }
            obj.insert(keyTok.text, QVariant::fromValue(child));
            break;
        }
        case TokenKind::Eof:
            return VDFObject(); // error: missing value
        case TokenKind::RBrace:
            return VDFObject(); // error: missing value before }
        }
    }
}

} // anonymous namespace

// ── Public API ───────────────────────────────────────────────────────

VDFObject parseVDF(const QByteArray& data)
{
    Parser parser(data);
    return parser.parse();
}

VDFObject parseVDFFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return VDFObject();

    QByteArray data = file.readAll();
    file.close();
    return parseVDF(data);
}

// ── Helper implementations ──────────────────────────────────────────

bool asObject(const QVariant& value, VDFObject& out)
{
    if (!value.isValid())
        return false;

    // QVariantMap is the type used for VDFObject
    if (value.canConvert<QVariantMap>()) {
        out = value.toMap();
        return true;
    }
    return false;
}

QString stringValue(const VDFObject& obj, const QString& key)
{
    auto it = obj.constFind(key);
    if (it == obj.constEnd())
        return QString();

    if (it->isValid() && it->canConvert<QString>())
        return it->toString();

    return QString();
}

QString stringValueFold(const VDFObject& obj, const QStringList& keys)
{
    // Exact match first
    for (const QString& key : keys) {
        auto it = obj.constFind(key);
        if (it != obj.constEnd() && it->canConvert<QString>()) {
            QString value = it->toString().trimmed();
            if (!value.isEmpty())
                return value;
        }
    }

    // Case-insensitive fallback
    for (const QString& key : keys) {
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
            if (it.key().compare(key, Qt::CaseInsensitive) == 0
                && it->canConvert<QString>()) {
                QString value = it->toString().trimmed();
                if (!value.isEmpty())
                    return value;
            }
        }
    }

    return QString();
}

bool objectValueFold(const VDFObject& obj, const QString& key, VDFObject& out)
{
    // Exact match
    auto it = obj.constFind(key);
    if (it != obj.constEnd() && asObject(it.value(), out))
        return true;

    // Case-insensitive fallback
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        if (it.key().compare(key, Qt::CaseInsensitive) == 0
            && asObject(it.value(), out))
            return true;
    }

    return false;
}

} // namespace ProtonSage
