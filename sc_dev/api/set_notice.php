<?php
include_once('common.php');
include_once('utils.php');

$type = trim(avoid_sql($_POST['type']));
$uid = trim(avoid_sql($_POST['uid']));
$ios_token = trim(avoid_sql($_POST['ios_token']));

$res = array();



if ( (strlen($uid) > 0)
	&& (strlen($type) > 0) ) {

    $conn = mysql_connect(DB_HOST, DB_USER, DB_PWD);
    if (!$conn) {
        $res = show_info('fail', '系统出错(2000)，请稍候重试 :)');
		echo json_encode($res);
		
        return 0;
    }

    mysql_query("set names utf8");
    mysql_select_db(DB_NAME, $conn);

	if ( $type == 'add' ) {
		if (strlen($ios_token) <= 0) {
			$res = show_info('fail', '请允许通知, 才能设置 :)');
			echo json_encode($res);
			return 0;
		}

		// 添加通知
		$sql = "INSERT IGNORE INTO sc_logined (uid, ios_token) values ('{$uid}', '{$ios_token}')";
		if ( !mysql_query($sql, $conn) ) {
			mysql_close($conn);

			$res = show_info('fail', '设置失败(9101).');
			$res['des'] = $sql;
			echo json_encode($res);

			return 0;
		}
		
	} else if ( $type == 'del') {
		// 删除通知
		$sql = "DELETE FROM sc_logined where uid = '{$uid}' LIMIT 1";
		if ( !mysql_query($sql, $conn) ) {
			mysql_close($conn);

			$res = show_info('fail', '设置失败(9101).');
			$res['des'] = $sql;
			echo json_encode($res);

			return 0;
		}
	}

	mysql_close($conn);
	
    $res = show_info('succ', '设置成功');
    echo json_encode($res);

    return 0;
	
} else {

	$res['status'] = 'fail';
	$res['des'] = 'parameters error';
}



echo json_encode($res);


?>
