#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <objbase.h>
#include <stdio.h>
#include <wchar.h>

static UINT sql(MSIHANDLE db, const wchar_t *query)
{
    MSIHANDLE view = 0;
    UINT code = MsiDatabaseOpenViewW(db, query, &view);
    if (!code) code = MsiViewExecute(view, 0);
    if (view) { MsiViewClose(view); MsiCloseHandle(view); }
    if (code) fwprintf(stderr, L"MSI SQL %u: %ls\n", code, query);
    return code;
}

int wmain(int argc, wchar_t **argv)
{
    MSIHANDLE db = 0, summary = 0;
    GUID guid;
    wchar_t product[40], package[40], component[40], query[2048];
    UINT code = ERROR_SUCCESS;
    const wchar_t *tables[] = {
        L"CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` CHAR(0) NOT NULL LOCALIZABLE PRIMARY KEY `Property`)",
        L"CREATE TABLE `Directory` (`Directory` CHAR(72) NOT NULL, `Directory_Parent` CHAR(72), `DefaultDir` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Directory`)",
        L"CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` SHORT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`)",
        L"CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64) LOCALIZABLE, `Description` CHAR(255) LOCALIZABLE, `Display` SHORT, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`)",
        L"CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)",
        L"CREATE TABLE `Registry` (`Registry` CHAR(72) NOT NULL, `Root` SHORT NOT NULL, `Key` CHAR(255) NOT NULL LOCALIZABLE, `Name` CHAR(255) LOCALIZABLE, `Value` CHAR(0) LOCALIZABLE, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Registry`)",
        L"CREATE TABLE `InstallExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT NOT NULL PRIMARY KEY `Action`)",
        L"CREATE TABLE `CustomAction` (`Action` CHAR(72) NOT NULL, `Type` SHORT NOT NULL, `Source` CHAR(72), `Target` CHAR(255) LOCALIZABLE PRIMARY KEY `Action`)"
    };
    const wchar_t *actions[] = {L"CostInitialize", L"FileCost", L"CostFinalize", L"InstallValidate", L"InstallInitialize",
        L"ProcessComponents", L"WriteRegistryValues", L"RegisterProduct", L"PublishFeatures", L"PublishProduct", L"InstallFinalize"};
    const unsigned int sequences[] = {800,900,1000,1400,1500,1600,5000,6100,6300,6400,6600};
    if (argc != 4 || GetFileAttributesW(argv[1]) != INVALID_FILE_ATTRIBUTES) return 64;
    if (wcscmp(argv[2],L"success") && wcscmp(argv[2],L"reboot") && wcscmp(argv[2],L"fail")) return 64;
    if (wcsncmp(argv[3],L"Software\\CiderExitFixture\\",wcslen(L"Software\\CiderExitFixture\\")) || wcschr(argv[3],L'\'') || wcschr(argv[3],L'\n') || wcschr(argv[3],L'\r')) return 64;
    if (FAILED(CoCreateGuid(&guid)) || !StringFromGUID2(&guid, product, 40)) return 65;
    if (FAILED(CoCreateGuid(&guid)) || !StringFromGUID2(&guid, package, 40)) return 65;
    if (FAILED(CoCreateGuid(&guid)) || !StringFromGUID2(&guid, component, 40)) return 65;
    if ((code = MsiOpenDatabaseW(argv[1], MSIDBOPEN_CREATE, &db))) goto done;
    for (size_t i=0; i<sizeof(tables)/sizeof(tables[0]); ++i) if ((code=sql(db,tables[i]))) goto done;
#define EXEC(q) do { if ((code=sql(db,q))) goto done; } while (0)
    EXEC(L"INSERT INTO `Directory` (`Directory`,`Directory_Parent`,`DefaultDir`) VALUES ('TARGETDIR',NULL,'SourceDir')");
    EXEC(L"INSERT INTO `Property` (`Property`,`Value`) VALUES ('ProductName','Cider Owned Exit Fixture')");
    EXEC(L"INSERT INTO `Property` (`Property`,`Value`) VALUES ('ProductVersion','1.0.0')");
    EXEC(L"INSERT INTO `Property` (`Property`,`Value`) VALUES ('ProductLanguage','1033')");
    EXEC(L"INSERT INTO `Property` (`Property`,`Value`) VALUES ('Manufacturer','Cider Fixture')");
    swprintf(query,2048,L"INSERT INTO `Property` (`Property`,`Value`) VALUES ('ProductCode','%ls')",product); EXEC(query);
    swprintf(query,2048,L"INSERT INTO `Component` (`Component`,`ComponentId`,`Directory_`,`Attributes`,`Condition`,`KeyPath`) VALUES ('Marker','%ls','TARGETDIR',4,NULL,'InstallMarker')",component); EXEC(query);
    EXEC(L"INSERT INTO `Feature` (`Feature`,`Feature_Parent`,`Title`,`Description`,`Display`,`Level`,`Directory_`,`Attributes`) VALUES ('Main',NULL,'Owned fixture',NULL,0,1,NULL,0)");
    EXEC(L"INSERT INTO `FeatureComponents` (`Feature_`,`Component_`) VALUES ('Main','Marker')");
    swprintf(query,2048,L"INSERT INTO `Registry` (`Registry`,`Root`,`Key`,`Name`,`Value`,`Component_`) VALUES ('InstallMarker',1,'%ls','Installed','#1','Marker')",argv[3]); EXEC(query);
    for (size_t i=0; i<sizeof(actions)/sizeof(actions[0]); ++i) {
        swprintf(query,2048,L"INSERT INTO `InstallExecuteSequence` (`Action`,`Condition`,`Sequence`) VALUES ('%ls',NULL,%u)",actions[i],sequences[i]); EXEC(query);
    }
    if (!wcscmp(argv[2],L"reboot")) EXEC(L"INSERT INTO `InstallExecuteSequence` (`Action`,`Condition`,`Sequence`) VALUES ('ScheduleReboot',NULL,6500)");
    if (!wcscmp(argv[2],L"fail")) {
        EXEC(L"INSERT INTO `CustomAction` (`Action`,`Type`,`Source`,`Target`) VALUES ('OwnedFailure',19,NULL,'Owned fixture failure')");
        EXEC(L"INSERT INTO `InstallExecuteSequence` (`Action`,`Condition`,`Sequence`) VALUES ('OwnedFailure',NULL,5100)");
    }
    if ((code=MsiGetSummaryInformationW(db,NULL,8,&summary))) goto done;
#define SUMMARY(id,type,value,text) do { if ((code=MsiSummaryInfoSetPropertyW(summary,id,type,value,NULL,text))) goto done; } while (0)
    SUMMARY(1,VT_I2,1252,NULL);
    SUMMARY(2,VT_LPSTR,0,L"Cider owned reference fixture");
    SUMMARY(7,VT_LPSTR,0,L"Intel;1033");
    SUMMARY(9,VT_LPSTR,0,package);
    SUMMARY(14,VT_I4,200,NULL);
    SUMMARY(15,VT_I4,0,NULL);
    if ((code=MsiSummaryInfoPersist(summary))) goto done;
    code=MsiDatabaseCommit(db);
done:
    if (summary) MsiCloseHandle(summary);
    if (db) MsiCloseHandle(db);
    printf("{\"kind\":\"owned-msi-build\",\"win32Error\":%u}\n",code);
    return code ? 1 : 0;
}
