struct BookmarkStore {
 static void deleteForFilePath(const std::string&,const char*){events.push_back("delete-bookmarks");}
 struct RenameMigration{bool prepared=false;bool uncertain=false;};
 static inline bool beginOk=true,commitOk=true,rollbackOk=true;
 static bool beginRenameMigration(const std::string&,const std::string&,const std::string&,const std::string&,const char*,RenameMigration&m,bool=false){events.push_back("bookmark-prepare");m.prepared=beginOk;return beginOk;}
 static bool commitRenameMigration(RenameMigration&){events.push_back("bookmark-commit");return commitOk;}
 static bool rollbackRenameMigration(RenameMigration&){events.push_back("bookmark-rollback");return rollbackOk;}
};
struct ClippingStore {
 static void deleteForFilePath(const std::string&,const char*){events.push_back("delete-clippings");}
 using RenameMigration=BookmarkStore::RenameMigration;
 static inline bool beginOk=true,commitOk=true,rollbackOk=true;
 static bool beginRenameMigration(const std::string&,const std::string&,const std::string&,const std::string&,const char*,RenameMigration&m,bool=false){events.push_back("clipping-prepare");m.prepared=beginOk;return beginOk;}
 static bool commitRenameMigration(RenameMigration&){events.push_back("clipping-commit");return commitOk;}
 static bool rollbackRenameMigration(RenameMigration&){events.push_back("clipping-rollback");return rollbackOk;}
};
