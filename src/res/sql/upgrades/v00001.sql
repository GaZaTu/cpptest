-- BEGIN AnalyticsError --
CREATE TABLE "AnalyticsError" (
  "id" BIT(128) NOT NULL,
  "type" VARCHAR(128) NOT NULL,
  "url" VARCHAR(512) NOT NULL,
  "userAgent" VARCHAR(512) NOT NULL,
  "body" JSON NOT NULL,
  "createdAt" TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY ("id")
);

--#ifdef USE_SQLITE
SELECT create_iso_timestamp_triggers('AnalyticsError', 'createdAt');
--#endif
-- END AnalyticsError --

-- BEGIN BlogEntry --
CREATE TABLE "BlogEntry" (
  "id" BIT(128) NOT NULL,
  "story" VARCHAR(256) NOT NULL,
  "title" VARCHAR(256) NOT NULL,
  "message" TEXT,
  "imageMimeType" VARCHAR(128),
  "imageFileExtension" VARCHAR(128),
  "createdAt" TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
  "updatedAt" TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE ("story", "title"),
  PRIMARY KEY ("id")
);

--#ifdef USE_SQLITE
SELECT create_iso_timestamp_triggers('BlogEntry', 'createdAt');
SELECT create_iso_timestamp_triggers('BlogEntry', 'updatedAt');
--#endif
-- END BlogEntry --

-- BEGIN Change --
CREATE TABLE "Change" (
  "id" BIT(128) NOT NULL,
  "kind" CHAR(1) CHECK ("kind" IN ('+','-','*')) NOT NULL,
  "targetEntity" VARCHAR(256) NOT NULL,
  "targetId" BIT(128),
  "targetColumn" VARCHAR(256),
  "newValue" TEXT,
  "createdAt" TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY ("id")
);

--#ifdef USE_SQLITE
SELECT create_iso_timestamp_triggers('Change', 'createdAt');
--#endif
-- END Change --

-- BEGIN UserRole --
CREATE TABLE "UserRole" (
  "id" BIT(128) NOT NULL,
  "name" VARCHAR(256) NOT NULL,
  "description" VARCHAR(512),
  UNIQUE ("name"),
  PRIMARY KEY ("id")
);
-- END UserRole --

-- BEGIN User --
CREATE TABLE "User" (
  "id" BIT(128) NOT NULL,
  "username" VARCHAR(256) NOT NULL,
  "password" BIT(256) NOT NULL,
  "createdAt" TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
  "updatedAt" TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE ("username"),
  PRIMARY KEY ("id")
);

--#ifdef USE_SQLITE
SELECT create_iso_timestamp_triggers('User', 'createdAt');
SELECT create_iso_timestamp_triggers('User', 'updatedAt');
--#endif
-- END User --

-- BEGIN N2M_User_UserRole --
CREATE TABLE "N2M_User_UserRole" (
  "userId" BIT(128) NOT NULL,
  "userRoleId" BIT(128) NOT NULL,
  FOREIGN KEY ("userId") REFERENCES "User" ("id") ON DELETE CASCADE,
  FOREIGN KEY ("userRoleId") REFERENCES "UserRole" ("id") ON DELETE CASCADE,
  PRIMARY KEY ("userId", "userRoleId")
);

CREATE INDEX "idx_N2M_User_UserRole_userId" ON "N2M_User_UserRole" ("userId");
CREATE INDEX "idx_N2M_User_UserRole_userRoleId" ON "N2M_User_UserRole" ("userRoleId");
-- END N2M_User_UserRole --

-- BEGIN TriviaCategory --
CREATE TABLE "TriviaCategory" (
  "id" BIT(128) NOT NULL,
  "name" VARCHAR(256) NOT NULL,
  "description" VARCHAR(512),
  "submitter" VARCHAR(256),
  "submitterUserId" BIT(128),
  "verified" BOOLEAN DEFAULT false,
  "disabled" BOOLEAN DEFAULT false,
  "createdAt" TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
  "updatedAt" TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
  "updatedByUserId" BIT(128),
  UNIQUE ("name"),
  FOREIGN KEY ("submitterUserId") REFERENCES "User" ("id") ON DELETE SET NULL,
  FOREIGN KEY ("updatedByUserId") REFERENCES "User" ("id") ON DELETE SET NULL,
  PRIMARY KEY ("id")
);

--#ifdef USE_SQLITE
SELECT create_iso_timestamp_triggers('TriviaCategory', 'createdAt');
SELECT create_iso_timestamp_triggers('TriviaCategory', 'updatedAt');
--#endif
-- END TriviaCategory --

-- BEGIN TriviaQuestion --
CREATE TABLE "TriviaQuestion" (
  "id" BIT(128) NOT NULL,
  "categoryId" BIT(128) NOT NULL,
  "question" VARCHAR(512) NOT NULL,
  "answer" VARCHAR(256) NOT NULL,
  "hint1" VARCHAR(256),
  "hint2" VARCHAR(256),
  "submitter" VARCHAR(256),
  "submitterUserId" BIT(128),
  "verified" BOOLEAN DEFAULT false,
  "disabled" BOOLEAN DEFAULT false,
  "createdAt" TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
  "updatedAt" TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
  "updatedByUserId" BIT(128),
--#ifdef USE_PGSQL
  "__tsvector" TSVECTOR,
--#endif
  FOREIGN KEY ("categoryId") REFERENCES "TriviaCategory" ("id") ON DELETE RESTRICT,
  FOREIGN KEY ("submitterUserId") REFERENCES "User" ("id") ON DELETE SET NULL,
  FOREIGN KEY ("updatedByUserId") REFERENCES "User" ("id") ON DELETE SET NULL,
  PRIMARY KEY ("id")
);

--#ifdef USE_SQLITE
SELECT create_iso_timestamp_triggers('TriviaQuestion', 'createdAt');
SELECT create_iso_timestamp_triggers('TriviaQuestion', 'updatedAt');
--#endif

CREATE INDEX "idx_TriviaQuestion_categoryId" ON "TriviaQuestion" ("categoryId");

--#ifdef USE_SQLITE
CREATE VIRTUAL TABLE "TriviaQuestionFTS" USING fts5 (
  "question",
  "answer",
  "hint1",
  "hint2",
  prefix = '3 4 5',
  tokenize = 'porter unicode61',
  content = 'TriviaQuestion'
);
SELECT create_fts_sync_triggers('TriviaQuestion', 'TriviaQuestionFTS');
--#ifdef USE_PGSQL
CREATE INDEX "idx_TriviaQuestion___tsvector" ON "TriviaQuestion" USING gist("__tsvector");

CREATE FUNCTION "trg_fn_TriviaQuestion___tsvector"()
RETURNS TRIGGER AS
$BODY$
BEGIN
  NEW."__tsvector" :=
    (setweight(to_tsvector(coalesce(NEW."question", '')), 'D')) ||
    (setweight(to_tsvector(coalesce(NEW."answer", '')), 'D')) ||
    (
      SELECT
        setweight(to_tsvector(coalesce("name", '')), 'D')
      FROM "TriviaCategory"
      WHERE "id" = NEW."categoryId"
    ) ||
    (setweight(to_tsvector(coalesce(NEW."hint1", '')), 'D')) ||
    (setweight(to_tsvector(coalesce(NEW."hint2", '')), 'D')) ||
    (setweight(to_tsvector(coalesce(NEW."submitter", '')), 'D'))
  ;

  RETURN NEW;
END
$BODY$
LANGUAGE 'plpgsql' VOLATILE;

CREATE TRIGGER "trg_TriviaQuestion___tsvector"
BEFORE INSERT OR UPDATE ON "TriviaQuestion" FOR EACH ROW
EXECUTE PROCEDURE "trg_fn_TriviaQuestion___tsvector"();
--#endif
-- END TriviaQuestion --

-- BEGIN TriviaReport --
CREATE TABLE "TriviaReport" (
  "id" BIT(128) NOT NULL,
  "questionId" BIT(128) NOT NULL,
  "message" VARCHAR(512) NOT NULL,
  "submitter" VARCHAR(256) NOT NULL,
  "submitterUserId" BIT(128),
  "createdAt" TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE ("questionId", "submitter"),
  FOREIGN KEY ("questionId") REFERENCES "TriviaQuestion" ("id") ON DELETE CASCADE,
  FOREIGN KEY ("submitterUserId") REFERENCES "User" ("id") ON DELETE SET NULL,
  PRIMARY KEY ("id")
);

--#ifdef USE_SQLITE
SELECT create_iso_timestamp_triggers('TriviaReport', 'createdAt');
--#endif
-- END TriviaReport --
