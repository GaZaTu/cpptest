--#ifdef USE_SQLITE
SELECT drop_fts_sync_triggers('TriviaQuestion');
DROP TABLE "TriviaQuestionFTS";

CREATE VIRTUAL TABLE "TriviaQuestionFTS" USING fts5 (
  "question",
  "answer",
  "hint1",
  "hint2",
  "submitter",
  "SELECT name FROM TriviaCategory WHERE id = $SRC.categoryId",
  prefix = '3 4 5',
  tokenize = 'porter unicode61',
  content = 'TriviaQuestion'
);
INSERT INTO "TriviaQuestionFTS" (
  "TriviaQuestionFTS",
  "rank"
) VALUES (
  'rank',
  'bm25(4, 4, 2, 2, 1, 1)'
);
SELECT create_fts_sync_triggers('TriviaQuestion', 'TriviaQuestionFTS');
--#ifdef USE_PGSQL
--#endif
