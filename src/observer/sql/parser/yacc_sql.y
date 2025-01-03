
%{

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utility>
#include <algorithm>

#include "common/log/log.h"
#include "common/lang/string.h"
#include "sql/parser/parse_defs.h"
#include "sql/parser/yacc_sql.hpp"
#include "sql/parser/lex_sql.h"
#include "sql/expr/expression.h"
#include "common/type/date_type.h"

using namespace std;

string token_name(const char *sql_string, YYLTYPE *llocp)
{
  return string(sql_string + llocp->first_column, llocp->last_column - llocp->first_column + 1);
}

int yyerror(YYLTYPE *llocp, const char *sql_string, ParsedSqlResult *sql_result, yyscan_t scanner, const char *msg)
{
  std::unique_ptr<ParsedSqlNode> error_sql_node = std::make_unique<ParsedSqlNode>(SCF_ERROR);
  error_sql_node->error.error_msg = msg;
  error_sql_node->error.line = llocp->first_line;
  error_sql_node->error.column = llocp->first_column;
  sql_result->add_sql_node(std::move(error_sql_node));
  return 0;
}

ArithmeticExpr *create_arithmetic_expression(ArithmeticExpr::Type type,
                                             Expression *left,
                                             Expression *right,
                                             const char *sql_string,
                                             YYLTYPE *llocp)
{
  ArithmeticExpr *expr = new ArithmeticExpr(type, left, right);
  expr->set_name(token_name(sql_string, llocp));
  return expr;
}

UnboundAggregateExpr *create_aggregate_expression(const char *aggregate_name,
                                           Expression *child,
                                           const char *sql_string,
                                           YYLTYPE *llocp)
{
  UnboundAggregateExpr *expr = new UnboundAggregateExpr(aggregate_name, child);
  expr->set_name(token_name(sql_string, llocp));
  return expr;
}

%}

%define api.pure full
%define parse.error verbose
/** 启用位置标识 **/
%locations
%lex-param { yyscan_t scanner }
/** 这些定义了在yyparse函数中的参数 **/
%parse-param { const char * sql_string }
%parse-param { ParsedSqlResult * sql_result }
%parse-param { void * scanner }

//标识tokens
%token  SEMICOLON
        BY
        CREATE
        DROP
        GROUP
        TABLE
        TABLES
        INDEX
        CALC
        SELECT
        DESC
        SHOW
        SYNC
        INSERT
        DELETE
        UPDATE
        LBRACE
        RBRACE
        COMMA
        TRX_BEGIN
        TRX_COMMIT
        TRX_ROLLBACK
        INT_T
        STRING_T
        FLOAT_T
        DATE_T
        TEXT_T
        HELP
        EXIT
        DOT //QUOTE
        INTO
        VALUES
        FROM
        WHERE
        AND
        SET
        ON
        LOAD
        DATA
        INFILE
        EXPLAIN
        STORAGE
        FORMAT
        UNIQUE
        IS_T
        NOT_T
        NULL_T
        NULLABLE
        IN_T
        INNER
        JOIN
        EQ
        LT
        GT
        LE
        GE
        NE
        ORDER
        ASC

/** union 中定义各种数据类型，真实生成的代码也是union类型，所以不能有非POD类型的数据 **/
%union {
  ParsedSqlNode *                            sql_node;
  ConditionSqlNode *                         condition;
  SelectSqlNode *                            sub_query;
  Value *                                    value;
  enum CompOp                                comp;
  RelAttrSqlNode *                           rel_attr;
  std::vector<AttrInfoSqlNode> *             attr_infos;
  AttrInfoSqlNode *                          attr_info;
  Expression *                               expression;
  std::vector<std::unique_ptr<Expression>> * expression_list;
  std::vector<Value> *                       value_list;
  std::vector<std::vector<Value>> *          insert_item_list;
  std::vector<ConditionSqlNode> *            condition_list;
  std::vector<RelAttrSqlNode> *              rel_attr_list;
  OrderBySqlNode *                           order_by_item;
  std::vector<OrderBySqlNode> *              order_by_list;
  OrderBySqlNode::OrderType                  order_type;
  char *                                     string;
  int                                        number;
  float                                      floats;
  bool                                       boolean;
  std::vector<std::vector<std::pair<std::string, std::vector<ConditionSqlNode> > > > * relation_list;
  std::vector<std::pair<std::string, std::vector<ConditionSqlNode> > > * relatoin_sub_item;
}
// std::vector<std::string> *                 relation_list;

%token <number> NUMBER
%token <floats> FLOAT
%token <string> ID
%token <string> SSS
//非终结符

/** type 定义了各种解析后的结果输出的是什么类型。类型对应了 union 中的定义的成员变量名称 **/
%type <number>              type
%type <condition>           condition
%type <value>               value
%type <number>              number
%type <comp>                comp_op
%type <rel_attr>            rel_attr
%type <attr_infos>          attr_def_list
%type <attr_info>           attr_def
%type <value_list>          value_list
%type <condition_list>      where
%type <condition_list>      condition_list
%type <string>              storage_format
%type <relation_list>       rel_list
%type <string>              aggregate_name
%type <expression>          expression
%type <expression_list>     expression_list
%type <expression_list>     group_by
%type <sql_node>            calc_stmt
%type <sql_node>            select_stmt
%type <value_list>          insert_item
%type <insert_item_list>    insert_item_list
%type <sql_node>            insert_stmt
%type <sql_node>            update_stmt
%type <sql_node>            delete_stmt
%type <sql_node>            create_table_stmt
%type <sql_node>            drop_table_stmt
%type <sql_node>            show_tables_stmt
%type <sql_node>            desc_table_stmt
%type <sql_node>            create_index_stmt
%type <sql_node>            drop_index_stmt
%type <sql_node>            sync_stmt
%type <sql_node>            begin_stmt
%type <sql_node>            commit_stmt
%type <sql_node>            rollback_stmt
%type <sql_node>            load_data_stmt
%type <sql_node>            explain_stmt
%type <sql_node>            set_variable_stmt
%type <sql_node>            help_stmt
%type <sql_node>            exit_stmt
%type <sql_node>            command_wrapper
// commands should be a list but I use a single command instead
%type <sql_node>            commands
%type <boolean>             nullable_desc
%type <order_type>          order_type
%type <order_by_item>       order_by_item
%type <order_by_list>       order_by_list
%type <order_by_list>       order_by
%type <sub_query>           sub_query_select_stmt
%type <relatoin_sub_item>   relation
%type <relatoin_sub_item>   inner_join_list
// %type <string>           relation

%left '+' '-'
%left '*' '/'
%nonassoc UMINUS
%%

commands: command_wrapper opt_semicolon  //commands or sqls. parser starts here.
  {
    std::unique_ptr<ParsedSqlNode> sql_node = std::unique_ptr<ParsedSqlNode>($1);
    sql_result->add_sql_node(std::move(sql_node));
  }
  ;

command_wrapper:
    calc_stmt
  | select_stmt
  | insert_stmt
  | update_stmt
  | delete_stmt
  | create_table_stmt
  | drop_table_stmt
  | show_tables_stmt
  | desc_table_stmt
  | create_index_stmt
  | drop_index_stmt
  | sync_stmt
  | begin_stmt
  | commit_stmt
  | rollback_stmt
  | load_data_stmt
  | explain_stmt
  | set_variable_stmt
  | help_stmt
  | exit_stmt
    ;

exit_stmt:      
    EXIT {
      (void)yynerrs;  // 这么写为了消除yynerrs未使用的告警。如果你有更好的方法欢迎提PR
      $$ = new ParsedSqlNode(SCF_EXIT);
    };

help_stmt:
    HELP {
      $$ = new ParsedSqlNode(SCF_HELP);
    };

sync_stmt:
    SYNC {
      $$ = new ParsedSqlNode(SCF_SYNC);
    }
    ;

begin_stmt:
    TRX_BEGIN  {
      $$ = new ParsedSqlNode(SCF_BEGIN);
    }
    ;

commit_stmt:
    TRX_COMMIT {
      $$ = new ParsedSqlNode(SCF_COMMIT);
    }
    ;

rollback_stmt:
    TRX_ROLLBACK  {
      $$ = new ParsedSqlNode(SCF_ROLLBACK);
    }
    ;

drop_table_stmt:    /*drop table 语句的语法解析树*/
    DROP TABLE ID {
      $$ = new ParsedSqlNode(SCF_DROP_TABLE);
      $$->drop_table.relation_name = $3;
      free($3);
    };

show_tables_stmt:
    SHOW TABLES {
      $$ = new ParsedSqlNode(SCF_SHOW_TABLES);
    }
    ;

desc_table_stmt:
    DESC ID  {
      $$ = new ParsedSqlNode(SCF_DESC_TABLE);
      $$->desc_table.relation_name = $2;
      free($2);
    }
    ;

create_index_stmt:    /*create index 语句的语法解析树*/
    CREATE INDEX ID ON ID LBRACE ID RBRACE
    {
      $$ = new ParsedSqlNode(SCF_CREATE_INDEX);
      CreateIndexSqlNode &create_index = $$->create_index;
      create_index.index_name = $3;
      create_index.relation_name = $5;
      create_index.attribute_name = $7;
      create_index.index_type = IndexMeta::IndexType::Normal;
      free($3);
      free($5);
      free($7);
    }
    | CREATE UNIQUE INDEX ID ON ID LBRACE ID RBRACE
    {
        $$ = new ParsedSqlNode(SCF_CREATE_INDEX);
        CreateIndexSqlNode &create_index = $$->create_index;
        create_index.index_name = $4;
        create_index.relation_name = $6;
        create_index.attribute_name = $8;
        create_index.index_type = IndexMeta::IndexType::Unique;
        free($4);
        free($6);
        free($8);
    }
    ;

drop_index_stmt:      /*drop index 语句的语法解析树*/
    DROP INDEX ID ON ID
    {
      $$ = new ParsedSqlNode(SCF_DROP_INDEX);
      $$->drop_index.index_name = $3;
      $$->drop_index.relation_name = $5;
      free($3);
      free($5);
    }
    ;

nullable_desc:
    // empty, not nullable by default
    {
        $$ = false;
    }
    |
    NULLABLE
    {
        $$ = true;
    } 
    | NULL_T
    {
        $$ = true;
    }
    | NOT_T NULL_T
    {
        $$ = false;
    }
    ;

create_table_stmt:    /*create table 语句的语法解析树*/
    CREATE TABLE ID LBRACE attr_def attr_def_list RBRACE storage_format
    {
      $$ = new ParsedSqlNode(SCF_CREATE_TABLE);
      CreateTableSqlNode &create_table = $$->create_table;
      create_table.relation_name = $3;
      free($3);

      std::vector<AttrInfoSqlNode> *src_attrs = $6;

      if (src_attrs != nullptr) {
        create_table.attr_infos.swap(*src_attrs);
        delete src_attrs;
      }
      create_table.attr_infos.emplace_back(*$5);
      std::reverse(create_table.attr_infos.begin(), create_table.attr_infos.end());
      delete $5;
      if ($8 != nullptr) {
        create_table.storage_format = $8;
        free($8);
      }
    }
    ;
attr_def_list:
    /* empty */
    {
      $$ = nullptr;
    }
    | COMMA attr_def attr_def_list
    {
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new std::vector<AttrInfoSqlNode>;
      }
      $$->emplace_back(*$2);
      delete $2;
    }
    ;
    
attr_def:
    ID type LBRACE number RBRACE nullable_desc
    {
      $$ = new AttrInfoSqlNode;
      $$->type = (AttrType)$2;
      $$->name = $1;
      $$->length = $4;
      $$->length++; // +1 for null tag
      $$->nullable = $6;
      free($1);
    }
    | ID type nullable_desc
    {
      $$ = new AttrInfoSqlNode;
      $$->type = (AttrType)$2;
      $$->name = $1;
      if($$->type == AttrType::DATES) {
        $$->length = sizeof(Date_t);
      } else {
        $$->length = 4;
      }
      $$->length++; // +1 for null tag
      $$->nullable = $3;
      free($1);
    }
    ;
number:
    NUMBER {$$ = $1;}
    ;
type:
    INT_T      { $$ = static_cast<int>(AttrType::INTS); }
    | STRING_T { $$ = static_cast<int>(AttrType::CHARS); }
    | FLOAT_T  { $$ = static_cast<int>(AttrType::FLOATS); }
    | DATE_T   { $$ = static_cast<int>(AttrType::DATES); }
    | TEXT_T   { $$ = static_cast<int>(AttrType::TEXTS); }
    ;

insert_item:
    LBRACE value value_list RBRACE
    {
        $$ = new std::vector<Value>();
        if($3 != nullptr) {
            $$->swap(*$3);
            delete $3;
        }
        $$->emplace_back(*$2);
        delete $2;
        std::reverse($$->begin(), $$->end());
    }
    ;

insert_item_list:
    // EMPTY
    {
        $$ = nullptr;
    }
    | COMMA insert_item insert_item_list
    {
        if($3 != nullptr) {
            $$ = $3;
        } else {
            $$ = new std::vector<std::vector<Value>>();
        }
        $$->emplace_back(*$2);
        delete $2;
    }
    ;

insert_stmt:        /*insert   语句的语法解析树*/
    INSERT INTO ID VALUES insert_item insert_item_list
    {
      $$ = new ParsedSqlNode(SCF_INSERT);
      $$->insertion.relation_name = $3;
      if ($6 != nullptr) {
        $$->insertion.items.swap(*$6);
        delete $6;
      }
      $$->insertion.items.emplace_back(*$5);
      delete $5;
      std::reverse($$->insertion.items.begin(), $$->insertion.items.end());
      free($3);
    }
    ;

value_list:
    /* empty */
    {
      $$ = nullptr;
    }
    | COMMA value value_list  { 
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new std::vector<Value>;
      }
      $$->emplace_back(*$2);
      delete $2;
    }
    ;
value:
    NUMBER {
      $$ = new Value((int)$1);
      @$ = @1;
    }
    | '-' NUMBER {
        $$ = new Value((int)-$2);
        @$ = @2;
    }
    |FLOAT {
      $$ = new Value((float)$1);
      @$ = @1;
    }
    | '-' FLOAT {
        $$ = new Value((float)-$2);
        @$ = @2;
    }
    |SSS {
      char *tmp = common::substr($1,1,strlen($1)-2);
      $$ = new Value(tmp);
      free(tmp);
      free($1);
    }
    | NULL_T {
        $$ = new Value();
        @$ = @1;
    }
    ;
storage_format:
    /* empty */
    {
      $$ = nullptr;
    }
    | STORAGE FORMAT EQ ID
    {
      $$ = $4;
    }
    ;
    
delete_stmt:    /*  delete 语句的语法解析树*/
    DELETE FROM ID where 
    {
      $$ = new ParsedSqlNode(SCF_DELETE);
      $$->deletion.relation_name = $3;
      if ($4 != nullptr) {
        $$->deletion.conditions.swap(*$4);
        delete $4;
      }
      free($3);
    }
    ;
update_stmt:      /*  update 语句的语法解析树*/
    UPDATE ID SET ID EQ value where 
    {
      $$ = new ParsedSqlNode(SCF_UPDATE);
      $$->update.relation_name = $2;
      $$->update.attribute_name = $4;
      $$->update.value = *$6;
      if ($7 != nullptr) {
        $$->update.conditions.swap(*$7);
        delete $7;
      }
      free($2);
      free($4);
    }
    ;
select_stmt:        /*  select 语句的语法解析树*/
    SELECT expression_list FROM rel_list where group_by order_by
    {
      $$ = new ParsedSqlNode(SCF_SELECT);
      if ($2 != nullptr) {
        $$->selection.expressions.swap(*$2);
        delete $2;
      }

      if ($4 != nullptr) {
        std::reverse($4->begin(), $4->end());
        $$->selection.relations.swap(*$4);
        delete $4;
      }

      if ($5 != nullptr) {
        $$->selection.conditions.swap(*$5);
        delete $5;
      }

      if ($6 != nullptr) {
        $$->selection.group_by.swap(*$6);
        delete $6;
      }

      if($7 != nullptr) {
        $$->selection.order_by.swap(*$7);
        delete $7;
      }
    }
    ;
calc_stmt:
    CALC expression_list
    {
      $$ = new ParsedSqlNode(SCF_CALC);
      $$->calc.expressions.swap(*$2);
      delete $2;
    }
    ;

expression_list:
    expression
    {
      $$ = new std::vector<std::unique_ptr<Expression>>;
      $$->emplace_back($1);
    }
    | expression COMMA expression_list
    {
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new std::vector<std::unique_ptr<Expression>>;
      }
      $$->emplace($$->begin(), $1);
    }
    ;

aggregate_name:
    ID { $$ = $1; }
    ;

expression:
    expression '+' expression {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::ADD, $1, $3, sql_string, &@$);
    }
    | expression '-' expression {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::SUB, $1, $3, sql_string, &@$);
    }
    | expression '*' expression {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::MUL, $1, $3, sql_string, &@$);
    }
    | expression '/' expression {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::DIV, $1, $3, sql_string, &@$);
    }
    | LBRACE expression RBRACE {
      $$ = $2;
      $$->set_name(token_name(sql_string, &@$));
    }
    | '-' expression %prec UMINUS {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::NEGATIVE, $2, nullptr, sql_string, &@$);
    }
    | value {
      $$ = new ValueExpr(*$1);
      $$->set_name(token_name(sql_string, &@$));
      delete $1;
    }
    | rel_attr {
      RelAttrSqlNode *node = $1;
      $$ = new UnboundFieldExpr(node->relation_name, node->attribute_name);
      $$->set_name(token_name(sql_string, &@$));
      delete $1;
    }
    | '*' {
      $$ = new StarExpr();
    }
    // your code here
    | aggregate_name LBRACE expression RBRACE {
        $$ = create_aggregate_expression($1, $3, sql_string, &@$);
    }
    | aggregate_name LBRACE expression COMMA expression_list RBRACE { // invalid
        $$ = create_aggregate_expression($1, nullptr, sql_string, &@$);
    }
    | aggregate_name LBRACE RBRACE { // invalid
        $$ = create_aggregate_expression($1, nullptr, sql_string, &@$);
    }
    ;

rel_attr:
    ID {
      $$ = new RelAttrSqlNode;
      $$->attribute_name = $1;
      free($1);
    }
    | ID DOT ID {
      $$ = new RelAttrSqlNode;
      $$->relation_name  = $1;
      $$->attribute_name = $3;
      free($1);
      free($3);
    }
    ;

relation:
    ID inner_join_list {
      $$ = $2;
      $$->emplace_back($1, std::vector<ConditionSqlNode>());
      free($1);
    }
    ;

inner_join_list:
    /* empty */
    {
      $$ = new std::vector<std::pair<std::string, std::vector<ConditionSqlNode> > >();
    }
    | INNER JOIN ID ON condition_list inner_join_list {
      $$ = $6;
      $$->emplace_back($3, std::move(*$5));
      free($3); // char *
    }
    ;

rel_list:
    relation {
      $$ = new std::vector<std::vector<std::pair<std::string, std::vector<ConditionSqlNode> > > >();
      std::reverse($1->begin(), $1->end());
      $$->emplace_back(std::move(*$1));
      // delete $1;
    }
    | relation COMMA rel_list {
      $$ = $3;
      std::reverse($1->begin(), $1->end());
      $$->emplace_back(std::move(*$1));
      // delete $1;
    }
    ;

where:
    /* empty */
    {
      $$ = nullptr;
    }
    | WHERE condition_list {
      $$ = $2;  
    }
    ;
condition_list:
    /* empty */
    {
      $$ = nullptr;
    }
    | condition {
      $$ = new std::vector<ConditionSqlNode>;
      $$->emplace_back(std::move(*$1));
      delete $1;
    }
    | condition AND condition_list {
      $$ = $3;
      $$->emplace_back(std::move(*$1));
      delete $1;
    }
    ;
condition:
    expression comp_op expression
    {
        $$ = new ConditionSqlNode;
        $$->left_type = ConditionSqlNode::SideType::Expr;
        $$->left_expression = std::unique_ptr<Expression>($1);
        $$->comp = $2;
        $$->right_type = ConditionSqlNode::SideType::Expr;
        $$->right_expression = std::unique_ptr<Expression>($3);
    }
    | expression comp_op LBRACE sub_query_select_stmt RBRACE
    {
        $$ = new ConditionSqlNode;
        $$->left_type = ConditionSqlNode::SideType::Expr;
        $$->left_expression = std::unique_ptr<Expression>($1);
        $$->comp = $2;
        $$->right_type = ConditionSqlNode::SideType::SubQuery;
        $$->right_sub_query = std::unique_ptr<SelectSqlNode>($4);
    }
    | LBRACE sub_query_select_stmt RBRACE comp_op expression
    {
        $$ = new ConditionSqlNode;
        $$->left_type = ConditionSqlNode::SideType::SubQuery;
        $$->left_sub_query = std::unique_ptr<SelectSqlNode>($2);
        $$->comp = $4;
        $$->right_type = ConditionSqlNode::SideType::Expr;
        $$->right_expression = std::unique_ptr<Expression>($5);
    }
    ;

sub_query_select_stmt:        /*  sub_query_select 语句的语法解析树*/
    SELECT expression_list FROM rel_list where
    {
      $$ = new SelectSqlNode();
      if ($2 != nullptr) {
        $$->expressions.swap(*$2);
        delete $2;
      }

      if ($4 != nullptr) {
        $$->relations.swap(*$4);
        delete $4;
      }

      if ($5 != nullptr) {
        $$->conditions.swap(*$5);
        delete $5;
      }
    }
    ;

comp_op:
      EQ { $$ = EQUAL_TO; }
    | LT { $$ = LESS_THAN; }
    | GT { $$ = GREAT_THAN; }
    | LE { $$ = LESS_EQUAL; }
    | GE { $$ = GREAT_EQUAL; }
    | NE { $$ = NOT_EQUAL; }
    | IS_T { $$ = IS; }
    | IS_T NOT_T { $$ = IS_NOT; }
    | IN_T { $$ = IN; }
    | NOT_T IN_T { $$ = NOT_IN; }

    ;

// your code here
group_by:
    /* empty */
    {
      $$ = nullptr;
    }
    | GROUP BY expression_list {
      $$ = $3;
    }
    ;

order_type:
    /* empty */
    {
        $$ = OrderBySqlNode::OrderType::ASC;
    }
    | ASC
    {
        $$ = OrderBySqlNode::OrderType::ASC;
    }
    | DESC
    {
        $$ = OrderBySqlNode::OrderType::DESC;
    }
    ;

order_by_item:
    rel_attr order_type
    {
        $$ = new OrderBySqlNode();
        RelAttrSqlNode *node = $1;
        $$->expr = std::make_unique<UnboundFieldExpr>(node->relation_name, node->attribute_name);
        $$->expr->set_name(token_name(sql_string, &@$));
        delete $1;
        $$ -> order_type = $2;
    }
    ;

order_by_list:
    order_by_item
    {
        $$ = new std::vector<OrderBySqlNode>();
        $$->emplace_back(std::move(*$1));
        delete $1;
    }
    | order_by_item COMMA order_by_list
    {
        $$ = $3;
        $$->insert($$->begin(), std::move(*$1));
        delete $1;
    }
    ;

order_by:
    /* empty */
    {
        $$ = nullptr;
    }
    | ORDER BY order_by_list
    {
        $$ = $3;
    }
    ;

load_data_stmt:
    LOAD DATA INFILE SSS INTO TABLE ID 
    {
      char *tmp_file_name = common::substr($4, 1, strlen($4) - 2);
      
      $$ = new ParsedSqlNode(SCF_LOAD_DATA);
      $$->load_data.relation_name = $7;
      $$->load_data.file_name = tmp_file_name;
      free($7);
      free(tmp_file_name);
    }
    ;

explain_stmt:
    EXPLAIN command_wrapper
    {
      $$ = new ParsedSqlNode(SCF_EXPLAIN);
      $$->explain.sql_node = std::unique_ptr<ParsedSqlNode>($2);
    }
    ;

set_variable_stmt:
    SET ID EQ value
    {
      $$ = new ParsedSqlNode(SCF_SET_VARIABLE);
      $$->set_variable.name  = $2;
      $$->set_variable.value = *$4;
      free($2);
      delete $4;
    }
    ;

opt_semicolon: /*empty*/
    | SEMICOLON
    ;
%%
//_____________________________________________________________________
extern void scan_string(const char *str, yyscan_t scanner);

int sql_parse(const char *s, ParsedSqlResult *sql_result) {
  yyscan_t scanner;
  yylex_init(&scanner);
  scan_string(s, scanner);
  int result = yyparse(s, sql_result, scanner);
  yylex_destroy(scanner);
  return result;
}
