#
# All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
# its licensors.
#
# For complete copyright and license terms please see the LICENSE at the root of this
# distribution (the "License"). All use of this software is governed by the License,
# or, if provided, by the license below or the license accompanying this file. Do not
# remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#

from __future__ import print_function
from abc import ABCMeta, abstractmethod
import metric_constant as c

STATE_CONTEXT_CRAWLER_ID = "crawler_name"
STATE_CONTEXT_QUERY = "query_parser"
STATE_CONTEXT_DDL = "ddl"

def db_query_id(crawler_name, index, key):
    return "{}_{}_{}".format(crawler_name,index,key)

def db_key(crawler_name, key):
    return "{}_{}".format(crawler_name,key)

#[ddl, drop, create, repair, cleanup]
class AbstractRecoveryState(object):
    __metaclass__ = ABCMeta     
    def __init__(self, context):
        self.__context = context          
        self.__max_retry = context[c.KEY_BACKOFF_MAX_TRYS] if c.KEY_BACKOFF_MAX_TRYS in context else 5
        self.__retry = 0
    
    @property
    def context(self):
        return self.__context

    @abstractmethod
    def execute(self, query_id, sync):
        pass

    def query(self, query_id, func, query, sync=False):
        id = None
        if query_id in self.context and self.context[query_id] :
            id = self.context[query_id]             
        else:
            id = self.context[query_id] = func(query, sync=False)                
            self.set(query_id, id)  
    
        query = self.context[c.KEY_ATHENA_QUERY].client.get_query_execution(id)
        print(
        "Attempt ", self.__retry, "for", query_id, 'is', query['Status']['State'], 'with id=>', id, self.__max_retry)
        if query['Status']['State'] == 'RUNNING' or query['Status']['State'] == 'QUEUED':              
            return None
        elif query['Status']['State'] == 'FAILED':
            print("The query '{}' FAILED with ERROR: {}".format(query, query['Status']["StateChangeReason"]))
            self.__retry += 1
        
        if self.__retry >= self.__max_retry:
            return id
        
        return id  

    def crawler_key(self, key):
        return db_key(self.context[STATE_CONTEXT_CRAWLER_ID], key)

    def set(self, key, value):
        self.context[c.KEY_DB].set(key, value)

"""
Generate the table schema
"""
class CreateDDL(AbstractRecoveryState):
    def __init__(self, context):
        AbstractRecoveryState.__init__(self, context)

    def execute(self, query_id, sync=False):  
        ddl_key = self.crawler_key(STATE_CONTEXT_DDL)  
        
        if ddl_key in self.context:
            print("Continuing table recovery.")
            return self.context[ddl_key]
        print("Generating table DDL")
        ddl = self.context[STATE_CONTEXT_QUERY]("SHOW CREATE TABLE {0}.{1}", result_as_list=False)        
        #table not present, return
        if len(ddl) == 0:
            print("Table not found.")
            return True        
        self.context[ddl_key] = self.define(ddl)          
        self.set(ddl_key,self.context[ddl_key])
        return ddl_key
    
    def define(self, ddl):    
        # drop table DDL properties - doing so corrects partition/schema alignment errors generated by GLUE    
        print("Spliting DDL on attribute TBLPROPERTIES")
    
        ddl_parts = ddl.split("TBLPROPERTIES")
        return "{} TBLPROPERTIES ('classification'='parquet', 'compressionType'='none', 'typeOfData'='file')".format(ddl_parts[0])    

"""
Drop table - easiest and quickest way to delete partitions, but this can easily take longer than the max lambda time
"""
class DropTable(AbstractRecoveryState):
    def __init__(self, context):
        AbstractRecoveryState.__init__(self, context)

    def execute(self, query_id, sync=False):  
        return self.query(query_id, self.context[STATE_CONTEXT_QUERY], "DROP TABLE {0}.{1}", sync=sync)

"""
Create the new table based on the table schema that was previously generated
"""
class CreateTable(AbstractRecoveryState):
    def __init__(self, context):
        AbstractRecoveryState.__init__(self, context)

    def execute(self, query_id, sync=False):  
        return self.query(query_id, self.context[c.KEY_ATHENA_QUERY].execute, self.context[self.crawler_key(STATE_CONTEXT_DDL)], sync=sync)

"""
Repair the table partitions - no data will appear in the table until the partitions are created
"""
class RepairTable(AbstractRecoveryState):
    def __init__(self, context):
        AbstractRecoveryState.__init__(self, context)
	
    def execute(self, query_id, sync=False):  
        return self.query(query_id, self.context[STATE_CONTEXT_QUERY], "MSCK REPAIR TABLE `{0}.{1}`", sync=sync)

"""
Delete the state information from Dynamodb
"""
class Cleanup(AbstractRecoveryState):
    def __init__(self, context):
        AbstractRecoveryState.__init__(self, context)

    def execute(self, query_id, sync=False):
        db_client = self.context[c.KEY_DB] 
        db_client.delete_item(self.context[STATE_CONTEXT_CRAWLER_ID])
        db_client.delete_item(self.crawler_key(STATE_CONTEXT_DDL))
        return True


"""
Validate the table data is correct
"""
class ValidateTableData(AbstractRecoveryState):
    def __init__(self, context):
        AbstractRecoveryState.__init__(self, context)
	
    def execute(self, query_id, sync=False):  
        return self.query(query_id, self.context[STATE_CONTEXT_QUERY], "SELECT count(uuid) as value FROM {0}.{1}", sync=sync)