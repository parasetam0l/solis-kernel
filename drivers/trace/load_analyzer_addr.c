
static unsigned int checking_phy_start_addr, checking_phy_last_addr, checking_phy_length;
static void __iomem *checking_phy_addr_iomem;
static unsigned int last_checking_value;

enum {
	FAR_MODE,
	UNTIL_MODE,
	ONLY_ONE_MODE,
};

int check_input_string_err(int input_format, const char *str) {

	int ret = 0, value;
	char *p_check_str;

	switch (input_format) {
		case FAR_MODE:
			p_check_str = strstr(str, "0x");
			if (p_check_str == NULL)
				goto err;

			ret = sscanf(p_check_str+2, "%x", &value);
			if (ret < 0)
				goto err;

			p_check_str = strstr(str, "++");
			if (p_check_str == NULL)
				goto err;

			p_check_str = strstr(p_check_str+2, "0x");
			if (p_check_str == NULL)
				goto err;

			ret = sscanf(p_check_str+2, "%x", &value);
			if (ret < 0)
				goto err;

			break;

		case UNTIL_MODE:
			p_check_str = strstr(str, "0x");
			if (p_check_str == NULL)
				goto err;

			ret = sscanf(p_check_str+2, "%x", &value);
			if (ret < 0)
				goto err;

			p_check_str = strstr(str, "--");
			if (p_check_str == NULL)
				goto err;

			p_check_str = strstr(p_check_str+2, "0x");
			if (p_check_str == NULL)
				goto err;

			ret = sscanf(p_check_str+2, "%x", &value);
			if (ret < 0)
				goto err;
			break;

		case ONLY_ONE_MODE:
			p_check_str = strstr(str, "0x");
			if (p_check_str == NULL)
				goto err;

			ret = sscanf(p_check_str+2, "%x", &value);
			if (ret < 0)
				goto err;
	}

	return ret;

err:
	return -1;
}

static ssize_t check_phy_addr_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret = 0;
	char *p_start_addr = NULL, *p_addr_length=NULL;
	int input_format;

	pr_info("user_buf size =%d\n", strlen(user_buf));

	/* STEP 1 : check address mode */
	if (strstr(user_buf, "++") != NULL)
		input_format = FAR_MODE;
	else if (strstr(user_buf, "--") !=NULL)
		input_format = UNTIL_MODE;
	else
		input_format = ONLY_ONE_MODE;

	/* STEP 2 : check error */
	ret = check_input_string_err(input_format, user_buf);
	if (ret < 0)
		goto err;

	/* STEP 2 : get addr info */
	switch (input_format) {
		case FAR_MODE:
			p_start_addr = strstr(user_buf, "0x");
			sscanf(p_start_addr + 2, "%x", &checking_phy_start_addr);

			p_addr_length = strstr(p_start_addr + 2, "0x");
			sscanf(p_addr_length + 2, "%x", &checking_phy_length);
			break;

		case UNTIL_MODE:
			p_start_addr = strstr(user_buf, "0x");
			sscanf(p_start_addr + 2, "%x", &checking_phy_start_addr);

			p_addr_length = strstr(p_start_addr + 2, "0x");
			sscanf(p_addr_length + 2, "%x", &checking_phy_last_addr);

			checking_phy_length = checking_phy_last_addr -checking_phy_start_addr + 4;
			break;

		case ONLY_ONE_MODE:
			p_start_addr = strstr(user_buf, "0x");
			sscanf(p_start_addr + 2, "%x", &checking_phy_start_addr);
			checking_phy_length = 4;
			break;
	}

	return count;

err:
	checking_phy_start_addr = 0;
	checking_phy_length = 0;

	return -EINVAL;
}

int phy_addr_read_sub(char *buf, int buf_size)
{
	int ret = 0, offset = 0;

	checking_phy_addr_iomem = ioremap(checking_phy_start_addr, checking_phy_length);
	if (checking_phy_addr_iomem == NULL)
		goto err;

	for (offset = 0; offset < checking_phy_length; offset += 4) {
		pr_info("show_addr=0x%x\n", checking_phy_start_addr + offset);

		last_checking_value = readl(checking_phy_addr_iomem + offset);

		ret +=  snprintf(buf + ret, buf_size - ret,
				"[0x%08x] 0x%08x\n", checking_phy_start_addr + offset, last_checking_value);
	}

	iounmap(checking_phy_addr_iomem);

	return ret;

err:
	return -EINVAL;
}

static ssize_t check_phy_addr_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos, phy_addr_read_sub);

	return size_for_copy;
}

static const struct file_operations check_phy_addr_fops = {
	.owner = THIS_MODULE,
	.read  = check_phy_addr_read,
	.write = check_phy_addr_write,
};

void debugfs_addr(struct dentry *d)
{
	if (!debugfs_create_file("check_phy_addr", 0600, d, NULL, &check_phy_addr_fops))
		pr_err("%s: debugfs_create_file, error\n", "check_phy_addr");
}
