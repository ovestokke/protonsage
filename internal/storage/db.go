package storage

import (
	"context"
	"database/sql"
	_ "embed"
	"errors"
	"fmt"
	"strings"

	_ "modernc.org/sqlite"
)

//go:embed schema.sql
var schemaSQL string

const driverName = "sqlite"

// DB wraps the SQLite handle used by ProtonSage storage code.
type DB struct {
	sql *sql.DB
}

type runner interface {
	ExecContext(context.Context, string, ...any) (sql.Result, error)
	QueryContext(context.Context, string, ...any) (*sql.Rows, error)
	QueryRowContext(context.Context, string, ...any) *sql.Row
}

// Queries contains concrete storage operations. It can run against a DB or tx.
type Queries struct {
	runner runner
}

// Open opens or creates a SQLite database at path and applies schema.sql.
func Open(path string) (*DB, error) {
	path = strings.TrimSpace(path)
	if path == "" {
		return nil, errors.New("open storage: database path is required")
	}
	return openDSN(path)
}

// OpenInMemory opens an in-memory SQLite database for tests and small tools.
func OpenInMemory() (*DB, error) {
	return openDSN(":memory:")
}

func openDSN(dsn string) (*DB, error) {
	sqlDB, err := sql.Open(driverName, dsn)
	if err != nil {
		return nil, fmt.Errorf("open sqlite database: %w", err)
	}
	// Keep one connection so in-memory DBs and connection-local pragmas are stable.
	sqlDB.SetMaxOpenConns(1)

	db := &DB{sql: sqlDB}
	if err := db.init(context.Background()); err != nil {
		_ = sqlDB.Close()
		return nil, err
	}
	return db, nil
}

func (db *DB) init(ctx context.Context) error {
	if _, err := db.sql.ExecContext(ctx, "PRAGMA foreign_keys = ON; PRAGMA busy_timeout = 5000;"); err != nil {
		return fmt.Errorf("configure sqlite pragmas: %w", err)
	}
	if _, err := db.sql.ExecContext(ctx, schemaSQL); err != nil {
		return fmt.Errorf("apply sqlite schema: %w", err)
	}
	return nil
}

// Close closes the underlying SQLite handle.
func (db *DB) Close() error {
	if db == nil || db.sql == nil {
		return nil
	}
	return db.sql.Close()
}

func (db *DB) queries() *Queries {
	return &Queries{runner: db.sql}
}

// WithTx runs fn inside a SQLite transaction. The transaction is rolled back on error.
func (db *DB) WithTx(ctx context.Context, fn func(*Queries) error) (err error) {
	tx, err := db.sql.BeginTx(ctx, nil)
	if err != nil {
		return fmt.Errorf("begin storage transaction: %w", err)
	}
	defer func() {
		if err != nil {
			_ = tx.Rollback()
		}
	}()

	if err = fn(&Queries{runner: tx}); err != nil {
		return err
	}
	if err = tx.Commit(); err != nil {
		return fmt.Errorf("commit storage transaction: %w", err)
	}
	return nil
}
