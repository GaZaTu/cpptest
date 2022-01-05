-- CREATE TABLE "Category" (
--   "id" BIT(16) NOT NULL,
--   "name" VARCHAR(100) NOT NULL,

--   PRIMARY KEY ("id")
-- );

-- CREATE TABLE "Test" (
--   "id" BIT(16) NOT NULL,
--   "count" BIT(16) NOT NULL,
--   "firstname" VARCHAR(100) NOT NULL,
--   "lastname" VARCHAR(100),
--   "age" INTEGER,
--   "timestamp" TIMESTAMPTZ,
--   "active" BOOLEAN NOT NULL DEFAULT true,
--   "created_at" TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
--   "image" BYTEA,

--   UNIQUE ("firstname"),
--   FOREIGN KEY ("category_id") REFERENCES "Category" ("id") ON DELETE RESTRICT,
--   PRIMARY KEY ("id")
-- );

-- SELECT create_iso_timestamp_triggers('Test', 'timestamp');
-- SELECT create_iso_timestamp_triggers('Test', 'created_at');

-- CREATE INDEX "idx_Test_category_id" ON "Test" ("category_id");

CREATE TABLE "Counter" (
  "id" BIT(16) NOT NULL,
  "twitch_user_id" VARCHAR(100) NOT NULL,
  "i" INTEGER NOT NULL DEFAULT 0,

  UNIQUE ("twitch_user_id"),
  PRIMARY KEY ("id")
)
